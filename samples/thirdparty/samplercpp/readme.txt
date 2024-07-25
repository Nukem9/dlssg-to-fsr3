Source: https://eheitzresearch.wordpress.com/762-2/

We provide our samplers optimized for N = 1, 2, 4, 8, 16, 32, 64, 128 and 256. Each sampler generates up to 256 spp in a 128x128 wrappable tile. The blue-noise distribution of the error is 
- optimal when spp=N,
- good when spp<N and spp is a power of two,
- cancelled when spp>N. 

Each sample has up to 256 dimensions. We optimized only the first eight dimensions by pairs, i.e. we computed four optimizations for D=2.

Each .cpp file provides:
- the Owen-scrambled Sobol sequence (256 samples x 256 dimensions). 
- the scrambling tile: 128x128x8.
- the ranking tile: 128x128x4. It is actually 128x128x8 but the data are currently duplicated. This can be optimized.
