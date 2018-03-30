/* All available tokens are defined here.
 * ET or emit token is a multipurpose macro
 * which should be defined before the point
 * where this file is included.
 */

// ,
ET(comma)

// .
ET(dot)

// :
ET(colon)

// !
ET(not)

// +
ET(plus)

// -
ET(minus)

// ^
ET(cap)

// /
ET(backslash)

// *
ET(star)

// {
ET(brace_open)

// }
ET(brace_close)

// (
ET(paranthesis_open)

// )
ET(paranthesis_close)

// >
ET(greater)

// >=
ET(greater_equal)

// <
ET(lesser)

// <=
ET(lesser_equal)

// =
ET(equal)

// ==
ET(equal_equal)

// !=
ET(not_equal)

// any consecutive sequence of integers with a decimal point in the middle
ET(number)

// any consecutive sequence of integers
ET(integer)

// end of file marker
ET(eof)

// any unknown token
ET(unknown)

// any consecutive sequence of characters between two "s
ET(string)

// any consecutive set of alphabets and/or numbers preceding with an alphabet
ET(identifier)

// keywords
#define KEYWORD(x, a) ET(x)
#include "keywords.h"
#undef KEYWORD
