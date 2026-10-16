from lark import Lark
import pycparser
import sys

dsl_parser = Lark.open("dsl.lark", rel_to=__file__, start='start', parser="lalr", propagate_positions=True)

if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        tree = dsl_parser.parse(f.read())
        print(tree.pretty())