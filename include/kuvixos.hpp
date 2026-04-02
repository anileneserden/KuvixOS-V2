#pragma once

#include <kuvixos.h>

class Console {
public:
	void Write(const char* text) const {
		print(text);
	}

	void WriteLine(const char* text) const {
		print(text);
		print("\n");
	}
};

class Args {
public:
	int Count() const {
		return kvx_argc();
	}

	const char* Get(int index) const {
		return kvx_argv(index);
	}
};