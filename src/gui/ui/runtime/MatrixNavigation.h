/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
namespace ingnomia::ui {
// Resolve the row's stable identity in the owner, then move focus without changing data.
inline std::pair<std::size_t,int> moveMatrixFocus(std::size_t row,int column,int dr,int dc,std::size_t rows,int columns){
 if(!rows||columns<=0)return {0,0};
 return {std::size_t(std::clamp<std::int64_t>(std::int64_t(row)+dr,0,std::int64_t(rows)-1)),std::clamp(column+dc,0,columns-1)};
}
}
