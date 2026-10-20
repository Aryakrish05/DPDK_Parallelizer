from __future__ import annotations
import sys 
from dataclasses import dataclass, field
from typing import List,Dict,Union,Set
from dsl_ast import _Expr,_Stmt,Lvalue,VarDecl,Assign,_MapExpr,Extract,Erase,Ret,IfStmt,ProcessDecl, Block
from visitor import Visitor

# TODO - generate the control and data dependencies and add functions for slicing
# TODO - use the above functions in the analyses
# TO BE DONE AFTER THE CODE GENERATION IS DONE

def get_used_lvalues(expr:_Expr):
    class LvalueCollector(Visitor):
        def __init__(self):
            self.lvalues = set()
        def visit_Lvalue(self,n:Lvalue):
            self.lvalues.add(tuple(n.names))
    cur_visitor = LvalueCollector()
    cur_visitor.visit(expr)
    return cur_visitor.lvalues

class Node:
    def __init__(self):
        self.pred = []
        self.defined = None 
        self.uses = set()
        self.access = None
    def print(self,indent):
        raise NotImplementedError
    def successors(self):
        return []

class Entry(Node):
    def __init__(self,next=None):
        super().__init__()
        self.next = next
    def successors(self):
        return [self.next]
    def print(self,indent):
        print(f"{' '*indent}Entry")
        
class Exit(Node):
    def __init__(self):
        super().__init__()
    def print(self,indent):
        print(f"{' '*indent}Exit")
        
class StmtNode(Node):
    def __init__(self,stmt,next):
        super().__init__()
        self.next = next
        self.stmt = stmt 
        # if(isinstance(stmt,Extract)):
        #     self.defined = stmt.pkt_field
        # elif(isinstance(stmt,VarDecl)):
        #     self.defined = tuple(stmt.var_name,)
        #     assert(stmt.expr is not None)
        #     self.uses = get_used_lvalues(stmt.expr)
        # elif(isinstance(stmt,Assign)):
        #     self.defined = tuple(stmt.var_name.names)
        #     self.uses = get_used_lvalues(stmt.expr)
        # elif(isinstance(stmt,Erase)):
        #     self.access = stmt
        #     self.uses = get_used_lvalues(stmt.key)

    def print(self,indent):
        print(f"{' '*indent}{self.stmt}")
            
    def successors(self):
        return [self.next]

class IfThenElseNode(Node):
    def __init__(self,stmt,true,false):
        super().__init__()
        self.stmt = stmt
        self.true = true
        self.false = false
    def successors(self):
        return [self.true,self.false]
    def print(self,indent):
        print(f"{' '*indent}If {self.stmt.cond}")

class RetNode(Node):
    def __init__(self,action,next):
        super().__init__()
        self.action = action
        self.next = next
    def successors(self):
        return [self.next]
    def print(self,indent):
        print(f"{' '*indent}Return {self.action}")


class CFG:
    def __init__(self,process:ProcessDecl):
        self.entry = Entry()
        self.exit = Exit()
        self.drop = RetNode("DROP",self.exit)
        self.forward = RetNode("FORWARD",self.exit)
        
        self.entry.next = self.build_block(process.block,None)
        
        #create topo sorted nodes list
        self.topo_sorted_nodes_list:List[Node] = []
        visited_set = set()
        def topo_sort(self,node:Node):
            visited_set.add(node)
            for succ in node.successors():
                if (succ is None):
                    raise Exception("not every path ends in a return")
                if (succ not in visited_set):
                    topo_sort(self,succ)
            self.topo_sorted_nodes_list.append(node)
        topo_sort(self,self.entry)
        self.topo_sorted_nodes_list.reverse()
        
        #fill preds
        for n in self.topo_sorted_nodes_list:
            for s in n.successors():
                #s is not none - asserted already in topo sorting
                s.pred.append(n)

    def build_block(self,block:Block,next):
        for stmt in reversed(block.stmts):
            next = self.build(stmt,next)
        return next
    
    def build(self,stmt,next):
        if(isinstance(stmt,Ret)):
            return (self.drop if stmt.action=="DROP" else self.forward)
        
        if(isinstance(stmt,IfStmt)):
            true = self.build_block(stmt.if_block,next)
            if(stmt.else_block is None):
                false = next 
            elif(isinstance(stmt.else_block,IfStmt)):
                false = self.build(stmt.else_block,next)
            else:
                false = self.build_block(stmt.else_block,next)
            return IfThenElseNode(stmt,true,false)
        
        return StmtNode(stmt,next)
    
    def print(self):
        def print_rec(node:Node,indent):
            node.print(indent)
            for succ in node.successors():
                print_rec(succ,indent+4)
        print_rec(self.entry,0)
        

# ipdom is the node which post dominates a particular node
# and is post dominated by nothing else 
# the lca of all the successors of a node in the post dominator tree 
# will be the post dominator of the node
def post_dominators(cfg:CFG):
    ipdom:Dict[Node,Union[Node,None]] = {cfg.exit:None}
    depth:Dict[Node,int] = {cfg.exit:0}
    
    def lca(a,b):
        while(a!=b):
            # a is the deeper one in the post dominator tree
            assert(a is not None and b is not None)
            if (depth[a]<depth[b]):
                a,b = b,a
            a = ipdom[a]
        return a 
            
    for n in reversed(cfg.topo_sorted_nodes_list):
        succs = n.successors()
        num_succs = len(succs)
        if (num_succs>0):
            d = succs[0]
            for i in range(1,num_succs):
                d = lca(d,succs[i])
            ipdom[n] = d
            assert(d is not None)
            depth[n] = depth[d] + 1
    
    return ipdom

# the nodes which have a **direct** control dependence on an IfThenElseNode
# are the nodes on the path from the successors to ipdom of the node
# nodes which are also IfThenElseNodes but control dependent on this node
# will be captured here but their branches will not be captured as
# they are not post dominators for both the IfThenElseNodes on their path

def control_deps(cfg:CFG):
    cdep:Dict[Node,Set[Node]] = {n:set() for n in cfg.topo_sorted_nodes_list}
    ipdom = post_dominators(cfg)
    for n in cfg.topo_sorted_nodes_list:
        if(isinstance(n,IfThenElseNode)):
            for s in n.successors():
                x = s
                while(x!=ipdom[n]):
                    assert(x is not None)
                    cdep[x].add(n)
                    x = ipdom[x]
    return cdep