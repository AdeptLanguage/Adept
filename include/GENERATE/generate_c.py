#!/usr/bin/python3

# Rather than explicitly write out the values for each token by hand,
# This file takes care of the dirty work of manually maintaining token
# IDs so that we can add/remove tokens more easily

# This file takes care of automatically sorting and positioning
# tokens in the correct order

# In order to add or remove tokens from the program,
# all you have to do is edit the 'tokens' array
# later in this file

# If you want multiple token names to share the
# same value, you can add token aliases by editing
# the 'token_aliases' array later in this file

from enum import IntEnum, auto, unique
import time
import os

@unique
class TokenType(IntEnum):
    NONE         = auto()
    WORD         = auto()
    KEYWORD      = auto()
    OPERATOR     = auto()
    LITERAL      = auto()
    POLYMORPH    = auto()
    PREPROCESSOR = auto()

@unique
class ExtraDataFormat(IntEnum):
    ID_ONLY    = auto()
    C_STRING   = auto()
    LEN_STRING = auto()
    MEMORY     = auto()

class Token:
    def __init__(self, short_name, token_type, extra_data_format, long_name):
        self.short_name = short_name
        self.token_type = token_type
        self.extra_data_format = extra_data_format
        self.long_name = short_name.replace("_", " ") if long_name == None else long_name
    
    def definition(self, value):
        return token_definition_string(self.short_name, value)

def token_definition_string(short_name, value):
    global tokens_max_short_name
    token_identifier = "TOKEN_{0}".format(short_name.upper())
    token_identifier = token_identifier.ljust(tokens_max_short_name + 6)
    return "#define {0} {1}\n".format(token_identifier, "0x%0.8X" % value)

tokens = [
    Token("none"                  , TokenType.NONE         , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("word"                  , TokenType.WORD         , ExtraDataFormat.C_STRING   , None                                ),
    Token("string"                , TokenType.LITERAL      , ExtraDataFormat.LEN_STRING , None                                ),
    Token("cstring"               , TokenType.LITERAL      , ExtraDataFormat.C_STRING   , None                                ),
    Token("add"                   , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("subtract"              , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("multiply"              , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("divide"                , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("assign"                , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("equals"                , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("notequals"             , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("lessthan"              , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("greaterthan"           , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("lessthaneq"            , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("greaterthaneq"         , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("not"                   , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("open"                  , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("close"                 , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("begin"                 , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("end"                   , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("newline"               , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("byte"                  , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("ubyte"                 , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("short"                 , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("ushort"                , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("int"                   , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("uint"                  , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("long"                  , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("ulong"                 , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("usize"                 , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("float"                 , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("double"                , TokenType.LITERAL      , ExtraDataFormat.MEMORY     , None                                ),
    Token("member"                , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("address"               , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("next"                  , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("bracket_open"          , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("bracket_close"         , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("modulus"               , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("generic_int"           , TokenType.LITERAL      , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("generic_float"         , TokenType.LITERAL      , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("add_assign"            , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("subtract_assign"       , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("multiply_assign"       , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("divide_assign"         , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("modulus_assign"        , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("bit_and_assign"        , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise and assign"                ),
    Token("bit_or_assign"         , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise or assign"                 ),
    Token("bit_xor_assign"        , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise xor assign"                ),
    Token("bit_lshift_assign"     , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise left shift assign"         ),
    Token("bit_rshift_assign"     , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise right shift assign"        ),
    Token("bit_lgc_lshift_assign" , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise logical left shift assign" ),
    Token("bit_lgc_rshift_assign" , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise logical right shift assign"),
    Token("ellipsis"              , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("uberand"               , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "uber and"                          ),
    Token("uberor"                , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "uber or"                           ),
    Token("terminate_join"        , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "terminate join"                    ),
    Token("colon"                 , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("bit_or"                , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise or"                        ),
    Token("bit_xor"               , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise xor"                       ),
    Token("bit_lshift"            , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise left shift"                ),
    Token("bit_rshift"            , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise right shift"               ),
    Token("bit_complement"        , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("bit_lgc_lshift"        , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise logical left shift"        ),
    Token("bit_lgc_rshift"        , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "bitwise logical right shift"       ),
    Token("associate"             , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("meta"                  , TokenType.PREPROCESSOR , ExtraDataFormat.C_STRING   , None                                ),
    Token("polymorph"             , TokenType.POLYMORPH    , ExtraDataFormat.C_STRING   , None                                ),
    Token("maybe"                 , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("increment"             , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("decrement"             , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("toggle"                , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("strong_arrow"          , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , "strong arrow"                      ),
    Token("range"                 , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("gives"                 , TokenType.OPERATOR     , ExtraDataFormat.ID_ONLY    , None                                ),
    Token("polycount"             , TokenType.LITERAL      , ExtraDataFormat.C_STRING   , None                                ),
    Token("POD"                   , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "POD keyword"                       ),
    Token("alias"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "alias keyword"                     ),
    Token("alignof"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "alignof keyword"                   ),
    Token("and"                   , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "and keyword"                       ),
    Token("as"                    , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "as keyword"                        ),
    Token("at"                    , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "at keyword"                        ),
    Token("break"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "break keyword"                     ),
    Token("case"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "case keyword"                      ),
    Token("cast"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "cast keyword"                      ),
    Token("class"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "class keyword"                     ),
    Token("const"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "const keyword"                     ),
    Token("constructor"           , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "constructor keyword"               ),
    Token("continue"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "continue keyword"                  ),
    Token("def"                   , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "def keyword"                       ),
    Token("default"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "default keyword"                   ),
    Token("defer"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "defer keyword"                     ),
    Token("define"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "define keyword"                    ),
    Token("delete"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "delete keyword"                    ),
    Token("each"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "each keyword"                      ),
    Token("else"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "else keyword"                      ),
    Token("embed"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "embed keyword"                     ),
    Token("enum"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "enum keyword"                      ),
    Token("exhaustive"            , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "exhaustive keyword"                ),
    Token("extends"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "extends keyword"                   ),
    Token("external"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "external keyword"                  ),
    Token("fallthrough"           , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "fallthrough keyword"               ),
    Token("false"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "false keyword"                     ),
    Token("for"                   , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "for keyword"                       ),
    Token("foreign"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "foreign keyword"                   ),
    Token("func"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "func keyword"                      ),
    Token("funcptr"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "funcptr keyword"                   ),
    Token("global"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "global keyword"                    ),
    Token("if"                    , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "if keyword"                        ),
    Token("implicit"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "implicit keyword"                  ),
    Token("import"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "import keyword"                    ),
    Token("in"                    , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "in keyword"                        ),
    Token("inout"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "inout keyword"                     ),
    Token("llvm_asm"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "llvm_asm keyword"                  ),
    Token("namespace"             , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "namespace keyword"                 ),
    Token("new"                   , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "new keyword"                       ),
    Token("null"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "null keyword"                      ),
    Token("or"                    , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "or keyword"                        ),
    Token("out"                   , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "out keyword"                       ),
    Token("override"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "override keyword"                  ),
    Token("packed"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "packed keyword"                    ),
    Token("pragma"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "pragma keyword"                    ),
    Token("private"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "private keyword"                   ),
    Token("public"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "public keyword"                    ),
    Token("record"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "record keyword"                    ),
    Token("repeat"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "repeat keyword"                    ),
    Token("return"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "return keyword"                    ),
    Token("sizeof"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "sizeof keyword"                    ),
    Token("static"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "static keyword"                    ),
    Token("stdcall"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "stdcall keyword"                   ),
    Token("struct"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "struct keyword"                    ),
    Token("switch"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "switch keyword"                    ),
    Token("thread_local"          , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "thread_local keyword"              ),
    Token("true"                  , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "true keyword"                      ),
    Token("typeinfo"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "typeinfo keyword"                  ),
    Token("typenameof"            , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "typenameof keyword"                ),
    Token("undef"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "undef keyword"                     ),
    Token("union"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "union keyword"                     ),
    Token("unless"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "unless keyword"                    ),
    Token("until"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "until keyword"                     ),
    Token("using"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "using keyword"                     ),
    Token("va_arg"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "va_arg keyword"                    ),
    Token("va_copy"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "va_copy keyword"                   ),
    Token("va_end"                , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "va_end keyword"                    ),
    Token("va_start"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "va_start keyword"                  ),
    Token("verbatim"              , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "verbatim keyword"                  ),
    Token("virtual"               , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "virtual keyword"                   ),
    Token("while"                 , TokenType.KEYWORD      , ExtraDataFormat.ID_ONLY    , "while keyword"                     )
]

# Calculate longest 'short name' of tokens
tokens_max_short_name = 0
for token in tokens:
    if tokens_max_short_name < len(token.short_name):
        tokens_max_short_name = len(token.short_name)

# Push keywords to bottom of list, and make sure they are sorted alphabetically
def is_not_keyword_otherwise_alphabetical(token):
    return "" if token.token_type is not TokenType.KEYWORD else token.short_name
tokens.sort(key=is_not_keyword_otherwise_alphabetical)

# Calculate first keyword value
beginning_of_keywords = 0
for i in range(0, len(tokens)):
    if tokens[i].token_type == TokenType.KEYWORD:
        beginning_of_keywords = i
        break

class TokenAlias:
    def __init__(self, short_name, long_name, points_to):
        self.short_name = short_name
        self.long_name = short_name.replace("_", " ") if long_name == None else long_name
        self.points_to = points_to
    
    def definition(self):
        for i in range(0, len(tokens)):
            if tokens[i].short_name == self.points_to:
                return token_definition_string(self.short_name, i)
        raise RuntimeError("TokenAlias.definition() failed to resolve destination '{0}'".format(self.short_name))

token_aliases = [
    TokenAlias("bit_and", "bitwise and", "address")
]

old_pkg_tokendata = """
// DEPRECATED: Pre-lexed files will probably be removed in the future.
// Used in place of common sequences in packages.
// Not recognized by parser.
#define TOKEN_PKG_MIN         TOKEN_PKG_WBOOL
#define TOKEN_PKG_WBOOL       0x0000000D0
#define TOKEN_PKG_WBYTE       0x0000000D1
#define TOKEN_PKG_WDOUBLE     0x0000000D2
#define TOKEN_PKG_WFLOAT      0x0000000D3
#define TOKEN_PKG_WINT        0x0000000D4
#define TOKEN_PKG_WLONG       0x0000000D5
#define TOKEN_PKG_WSHORT      0x0000000D6
#define TOKEN_PKG_WUBYTE      0x0000000D7
#define TOKEN_PKG_WUINT       0x0000000D8
#define TOKEN_PKG_WULONG      0x0000000D9
#define TOKEN_PKG_WUSHORT     0x0000000DA
#define TOKEN_PKG_WUSIZE      0x0000000DB
#define TOKEN_PKG_MAX         TOKEN_PKG_WUSIZE
"""

extra_data_format_encode_offset = ord('a') - 1

def generate_header(filename):
    head = "\n// This file was auto-generated by 'include/TOKEN/generate_c.py'\n\n#ifndef _ISAAC_TOKEN_DATA_H\n#define _ISAAC_TOKEN_DATA_H\n\n"
    iteration_version = "#define TOKEN_ITERATION_VERSION 0x%0.8X\n\n" % int(time.time())
    tail = old_pkg_tokendata + "\n#endif // _ISAAC_TOKEN_DATA_H\n"
    
    f = open(filename, "w")
    f.write(head)
    f.write(iteration_version)
    for i in range(0, len(tokens)):
        token = tokens[i]
        f.write(token.definition(i))
    for token_alias in token_aliases:
        f.write(token_alias.definition())
    f.write("\n");
    f.write("#define MAX_LEX_TOKEN 0x%0.8X\n" % (len(tokens) - 1))
    f.write("#define BEGINNING_OF_KEYWORD_TOKENS 0x%0.8X\n" % beginning_of_keywords)
    f.write("\n");
    f.write("#define TOKEN_EXTRA_DATA_FORMAT_ID_ONLY    0x%0.8X\n" % int(extra_data_format_encode_offset + ExtraDataFormat.ID_ONLY))
    f.write("#define TOKEN_EXTRA_DATA_FORMAT_C_STRING   0x%0.8X\n" % int(extra_data_format_encode_offset + ExtraDataFormat.C_STRING))
    f.write("#define TOKEN_EXTRA_DATA_FORMAT_LEN_STRING 0x%0.8X\n" % int(extra_data_format_encode_offset + ExtraDataFormat.LEN_STRING))
    f.write("#define TOKEN_EXTRA_DATA_FORMAT_MEMORY     0x%0.8X\n" % int(extra_data_format_encode_offset + ExtraDataFormat.MEMORY))
    f.write("\n");
    f.write("extern const char *global_token_name_table[];\n");
    f.write("extern const char global_token_extra_format_table[];\n");
    f.write("\n");
    f.write("extern const char *global_token_keywords_list[];\n");
    f.write("extern unsigned long long global_token_keywords_list_length;\n");
    f.write(tail)
    f.close()
    print("[done] Generated token_data.h")

def generate_source(filename):
    head = "\n// This file was auto-generated by 'include/TOKEN/generate_c.py'\n\n"

    longest_long_name = 0
    for token in tokens:
        if longest_long_name < len(token.long_name):
            longest_long_name = len(token.long_name)
    
    f = open(filename, "w");
    f.write(head)
    f.write("const char *global_token_name_table[] = {\n");
    for i in range(0, len(tokens)):
        token = tokens[i]
        padding = " " * (longest_long_name - len(token.long_name) + 1)
        comment = "// 0x%0.8X" % i
        comment_and_padding = padding + comment
        f.write("    \"" + token.long_name + "\"" + ("," if i != len(tokens) else "") + comment_and_padding + "\n");
    f.write("};\n");
    f.write("\n");
    f.write("const char global_token_extra_format_table[] = \"");
    for i in range(0, len(tokens)):
        token = tokens[i]
        f.write(chr(extra_data_format_encode_offset + int(token.extra_data_format)));
    f.write("\";\n");
    f.write("\n");
    num_keywords = 0
    f.write("const char *global_token_keywords_list[] = {\n");
    for i in range(0, len(tokens)):
        token = tokens[i]
        if token.token_type != TokenType.KEYWORD:
            continue
        f.write("    \"" + token.short_name + "\"" + ("," if i != len(tokens) else "") + "\n");
        num_keywords += 1
    f.write("};\n");
    f.write("\n");
    f.write("unsigned long long global_token_keywords_list_length = {0};\n".format(num_keywords));
    f.close()
    print("[done] Generated token_data.c")


def main():
    cwd = os.getcwd()
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    generate_header("../../include/TOKEN/token_data.h")
    generate_source("../../src/TOKEN/token_data.c")
    os.chdir(cwd)

main()
