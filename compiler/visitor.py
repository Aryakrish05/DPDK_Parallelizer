from dataclasses import is_dataclass


class Visitor:
    def visit(self, node):
        if isinstance(node, (list, tuple)):
            for item in node:
                self.visit(item)
        elif is_dataclass(node):
            getattr(self, "visit_" + type(node).__name__, self.generic_visit)(node)
        #else:
        #    print(f"#########{type(node)}########\n", node)

    def generic_visit(self, node):
        for value in vars(node).values():
            self.visit(value)
