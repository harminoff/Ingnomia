#pragma once

#include <iostream>
#include <string_view>

class TestContext
{
public:
	void check( bool condition, std::string_view expression, std::string_view file, int line )
	{
		if( condition ) return;
		++failures_;
		std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
	}

	[[nodiscard]] int result() const noexcept { return failures_ == 0 ? 0 : 1; }

private:
	int failures_{};
};

#define CHECK( Context, Expression ) ( Context ).check( static_cast<bool>( Expression ), #Expression, __FILE__, __LINE__ )
