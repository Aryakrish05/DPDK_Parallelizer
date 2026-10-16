from lark import Lark, logger
import logging
import sys
from lark import ast_utils
from dsl_ast import *

logger.setLevel(logging.DEBUG)
dsl_parser = Lark.open("dsl.lark", rel_to=__file__, start='start', parser="lalr", propagate_positions=True, debug=True)
transformer = ast_utils.create_transformer(this_module,DSLTransformer())

if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        tree = dsl_parser.parse(f.read())
        print(transformer.transform(tree))
        