# https://lark-parser.readthedocs.io/en/latest/examples/advanced/create_ast.html
import sys 
from typing import Any, List, Optional
from dataclasses import *

from lark import ast_utils, Transformer, v_args
from lark.tree import Meta

this_module = sys.modules[__name__]

class _Ast(ast_utils.Ast):
    pass

# Types

class _Type(_Ast):
    pass

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

# Atoms 

@dataclass
class BooleanLiteral(_Ast):
    value: str
    
@dataclass
class IntegerLiteral(_Ast):
    value: str
    
@dataclass
class Lvalue(_Ast,ast_utils.AsList):
    #eg [phv,eth,proto]
    names: List[str]


class StructExpr(_Ast):
    pass

    
@dataclass
class EnumDecl(_Ast):
    name: str
    items: List[tuple[str, Optional[IntegerLiteral]]]
    
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
    found_block: None #fill this with the type for block
    not_found_block: None #fill this with the type for block

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
    block: Any

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
