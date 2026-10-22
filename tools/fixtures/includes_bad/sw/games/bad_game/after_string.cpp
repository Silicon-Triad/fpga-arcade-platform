// A "/*" inside a string literal must not hide the include after it.
const char* kSlashStar = "/*";
#include "hal/register_map.hpp"
