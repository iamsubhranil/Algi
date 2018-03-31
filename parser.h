#pragma once

#include "lexer.h"
#include "stmt.h"
#include <stdint.h>
#include <stdbool.h>

BlockStatement parse(TokenList list);
uint64_t parser_has_errors();
