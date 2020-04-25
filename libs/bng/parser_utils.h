#ifndef LIBS_BNG_PARSER_UTILS_H_
#define LIBS_BNG_PARSER_UTILS_H_

#include <ostream>

namespace BNG {

class ASTBaseNode;

std::ostream& errs();
std::ostream& errs(const ASTBaseNode* loc);

double convert_to_dbl(const char* str);
long long convert_dec_to_llong(const char* str);

// same as strdup, only uses 'new' to allocate memory so that we can
// consistently use 'delete' afterwards to free it
char* strdup_new(const char* str);

} // namespace BNG

#endif // LIBS_BNG_PARSER_UTILS_H_