# vectormath

## Current Version
commit ee960fad0a4bbbf0ee2e7d03fc749c49ebeefaef	of https://github.com/glampert/vectormath.git
Last updated Jan 3, 2017

## How to update
1. Download the latest code from https://github.com/glampert/vectormath.git
1. Unzip source
1. Open vectormath.hpp
1. Replace "using namespace Vectormath::SSE;" (~line 35) with 
	namespace Vectormath
	{
		using namespace SSE;
	}
1. Replace "using namespace Vectormath::Scalar;" (~line 40) with 
	namespace Vectormath
	{
		using namespace Scalar;
	}
1. Replace "using namespace Vectormath;" (~line 47) with 
	namespace math
	{
		using namespace Vectormath;
	}
1. Update this `CAULDRONREADME.md` with the new version information!
