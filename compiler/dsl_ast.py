# https://lark-parser.readthedocs.io/en/latest/examples/advanced/create_ast.html
from __future__ import annotations
import sys 
from typing import List, Optional, Union, Tuple
from dataclasses import *

from lark import ast_utils, Transformer, v_args

this_module = sys.modules[__name__]

class _Ast(ast_utils.Ast):
    pass

# Types

class _Type(_Ast):
    pass

@dataclass
class Start(_Ast,ast_utils.AsList):
    decls: List[Union[MapDecl,StructDecl,ProcessDecl,ConstDecl,EnumDecl]]
    
@dataclass
class UintType(_Type):
    size: int

@dataclass 
class BoolType(_Type):
   pass

@dataclass
class StructType(_Type):
    name: str

@dataclass
class EnumType(_Type):
    name: str

# Expressions
class _Expr(_Ast):
    pass

@dataclass
class BooleanLiteral(_Expr):
    value: str
    
@dataclass
class IntegerLiteral(_Expr):
    value: str
    
@dataclass
class Lvalue(_Expr,ast_utils.AsList):
    #eg [phv,eth,proto]
    names: List[str]

@dataclass
class StructExpr(_Expr):
    struct_type: StructType
    field_inits: List[Tuple[str,_Expr]]

@dataclass
class BracketedExpr(_Expr):
    expr: _Expr

@dataclass
class Lnotop(_Expr):
    expr: _Expr

@dataclass
class Bnotop(_Expr):
    expr: _Expr

@dataclass
class Minusop(_Expr):
    expr: _Expr

@dataclass
class Binop(_Expr):
    lexpr: _Expr
    op: str
    rexpr: _Expr
    
# map expressions
class _MapExpr(_Ast):
    pass

@dataclass
class MapGet(_MapExpr):
    value_type: _Type
    value_name: str 
    map_name: str
    key: _Expr
    
@dataclass
class MapPut(_MapExpr):
    map_name: str 
    key: _Expr
    value:_Expr

@dataclass
class MapRmw(_MapExpr):
    map_name: str 
    key: _Expr
    rmw_op: str
    args: List[_Expr]
    
# Statements
class _Stmt(_Ast):
    pass

@dataclass
class Block(_Ast,ast_utils.AsList):
    stmts: List[_Stmt]
    
@dataclass
class VarDecl(_Stmt):
    var_type: _Type
    var_name: str 
    expr: Optional[_Expr]

@dataclass
class Assign(_Stmt):
    var_name: Lvalue
    expr: _Expr

@dataclass
class Erase(_Stmt):
    map_name: str
    key: _Expr

@dataclass
class Ret(_Stmt):
    action: str

@dataclass
class Extract(_Stmt):
    pkt_field: Lvalue

@dataclass
class IfStmt(_Stmt):
    cond: Union[_Expr,_MapExpr] 
    if_block: Block
    else_block: Optional[Union[IfStmt,Block]]

@dataclass
class EnumDecl(_Ast):
    name: str
    items: List[tuple[str, Optional[IntegerLiteral]]]

@dataclass
class ConstDecl(_Ast):
    const_type: _Type
    name: str
    value: _Expr
@dataclass
class StructDecl(_Ast):
    name: str
    fields: List[tuple[_Type, str]]
    
@dataclass 
class RmwDef(_Ast):
    name: str
    elem_type: _Type    
    elem_name: str
    param_list: List[tuple[_Type,str]]
    found_block: Block
    not_found_block: Block

@dataclass
class MapDecl(_Ast):
    key_type: _Type
    value_type: _Type
    lock_type: str
    map_size: str
    impl: str
    header: Optional[str]
    name: str
    rmw_ops: Optional[List[RmwDef]]
    
@dataclass
class ProcessDecl(_Ast):
    phv_type: StructType
    phv_name: str
    block: Block

class DSLTransformer(Transformer):
    
    def UINT(self,s):
        #uintN_t
        return int(s[4:-2])
    
    #store all other literals as strings
    def NAME(self,s):
        return str(s)
    def LOCK_TYPE(self,s):
        return str(s)
    def INT(self,n):
        return str(n)
    def HEX(self,n):
        return str(n)
    def BOOL(self,n):
        return str(n)
    def STRING(self,s):
        return str(s)[1:-1]
    def RMW_NAME(self,s):
        return str(s)
    def PACKET_ACTION(self,s):
        return str(s)
    def BINOP(self,s):
        return str(s)
    
    # By default, transformers work on the content themselves
    
    # Enum Decl
    # args consists of [NAME,integer_literal or None]
    def enum_item(self,args):
        return (args[0],args[1])
    # args consists of [(NAME,integer_literal or None),...]
    def enum_items(self,args):
        return args
    
    # Struct Decl
    def field(self,args):
        return (args[0],args[1])
    def fields(self,args):
        return args
    
    # Map Decl
    def map_size(self,n):
        return n[0]
    def rmw_ops(self,args):
        return args
    def param(self,args):
        return (args[0],args[1])
    def param_list(self,args):
        return args

    def args_list(self,args):
        return args
    
    def field_init(self,args):
        return (args[0],args[1])
    
    def field_inits(self,args):
        return args