#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
namespace juce {
template <typename T> struct MathConstants { static constexpr T pi = (T)3.1415926535897932384626433832795; static constexpr T twoPi = (T)6.283185307179586476925286766559; };
template <typename T> constexpr T jlimit(T lo, T hi, T v) { return std::min(hi, std::max(lo, v)); }
template <typename T> constexpr T jmax(T a, T b) { return std::max(a,b); }
template <typename T> constexpr T jmin(T a, T b) { return std::min(a,b); }
namespace dsp {
template <typename T> struct Complex { T real{}, imag{}; Complex()=default; Complex(T r,T i):real(r),imag(i){} };
class FFT {
public:
 explicit FFT(int order): n(1<<order) {}
 void perform(const Complex<float>* src, Complex<float>* dst, bool inverse) const {
   std::vector<std::complex<float>> a((size_t)n);
   for(int i=0;i<n;++i) a[(size_t)i] = {src[i].real,src[i].imag};
   for(int i=1,j=0;i<n;++i){ int bit=n>>1; for(;j&bit;bit>>=1) j^=bit; j^=bit; if(i<j) std::swap(a[(size_t)i],a[(size_t)j]); }
   for(int len=2;len<=n;len<<=1){ const float ang=(inverse?2.0f:-2.0f)*3.14159265358979323846f/(float)len; std::complex<float> root(std::cos(ang),std::sin(ang)); for(int i=0;i<n;i+=len){ std::complex<float> w(1,0); for(int j=0;j<len/2;++j){ auto u=a[(size_t)(i+j)]; auto v=a[(size_t)(i+j+len/2)]*w; a[(size_t)(i+j)]=u+v; a[(size_t)(i+j+len/2)]=u-v; w*=root; } } }
   const float scale=inverse?1.0f/(float)n:1.0f;
   for(int i=0;i<n;++i){ dst[i].real=a[(size_t)i].real()*scale; dst[i].imag=a[(size_t)i].imag()*scale; }
 }
private: int n;
};
}
}
