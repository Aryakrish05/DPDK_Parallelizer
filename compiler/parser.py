from lark import Lark, logger
import logging
import sys
from lark import ast_utils
from dsl_ast import *
from visitor import *
from cfg import *

logger.setLevel(logging.DEBUG)
dsl_parser = Lark.open("dsl.lark", rel_to=__file__, start='start', parser="lalr", propagate_positions=True, debug=True)
transformer = ast_utils.create_transformer(this_module,DSLTransformer())

if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        tree = dsl_parser.parse(f.read())
        ast = transformer.transform(tree)
        class GetProcessVisitor(Visitor):
            def __init__(self) -> None:
                self.process = None
            def visit_ProcessDecl(self,n):
                self.process = n                
        v = GetProcessVisitor()
        v.visit(ast)
        assert(v.process is not None)
        cfg1 = CFG(v.process)
        #cfg1.print()
        # ipdom = post_dominators(cfg1)
        # for key,value in ipdom.items():
        #     key.print(0)
        #     if value is None:
        #         print(' '*10+"None")
        #     else:
        #         value.print(10)
        cdeps = control_deps(cfg1)
        for key,values in cdeps.items():
            key.print(10)
            for v in values:
                v.print(0)
            
            
        