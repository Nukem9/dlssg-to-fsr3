#pragma once

#define FFX_RETURN_ON_FAIL(x)   \
	do                          \
	{                           \
		auto __status = (x);    \
		if (__status != FFX_OK) \
		{                       \
			return __status;    \
		}                       \
	} while (0);

#define FF_SUCCEEDED(x) ((x) == FFX_OK)
