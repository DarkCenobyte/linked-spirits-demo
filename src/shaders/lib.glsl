// ---- shared library: hashing, noise, SDF operators --------------------------
#define PI 3.14159265
#define ST U[0].y
float h1(float n){return fract(sin(n)*43758.5453);}
float h2(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
vec3 h3(float n){return fract(sin(vec3(n,n+1.7,n+3.1))*vec3(43758.5453,22578.1459,19642.3490));}
float vn(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);float n=dot(i,vec3(1,57,113));
 return mix(mix(mix(h1(n),h1(n+1.),f.x),mix(h1(n+57.),h1(n+58.),f.x),f.y),mix(mix(h1(n+113.),h1(n+114.),f.x),mix(h1(n+170.),h1(n+171.),f.x),f.y),f.z);}
float fbm(vec3 p){float a=.5,s=0.;for(int i=0;i<5;i++){s+=a*vn(p);p=p*2.03+vec3(1.7,9.2,3.1);a*=.5;}return s;}
float vn2(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(h2(i),h2(i+vec2(1,0)),f.x),mix(h2(i+vec2(0,1)),h2(i+1.),f.x),f.y);}
mat2 rot(float a){float c=cos(a),s=sin(a);return mat2(c,s,-s,c);}
float smin(float a,float b,float k){float h=clamp(.5+.5*(b-a)/k,0.,1.);return mix(b,a,h)-k*h*(1.-h);}
float smax(float a,float b,float k){return -smin(-a,-b,k);}
float sph(vec3 p,float r){return length(p)-r;}
// ellipsoid (approximate but well behaved)
float ell(vec3 p,vec3 r){float k0=length(p/r),k1=length(p/(r*r));return k0*(k0-1.)/k1;}
float cap(vec3 p,vec3 a,vec3 b,float r){vec3 pa=p-a,ba=b-a;float h=clamp(dot(pa,ba)/dot(ba,ba),0.,1.);return length(pa-ba*h)-r;}
// capsule with radius varying from ra (at a) to rb (at b)
float capr(vec3 p,vec3 a,vec3 b,float ra,float rb){vec3 pa=p-a,ba=b-a;float h=clamp(dot(pa,ba)/dot(ba,ba),0.,1.);return length(pa-ba*h)-mix(ra,rb,h);}
float box(vec3 p,vec3 b,float r){vec3 q=abs(p)-b+r;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.)-r;}
float tor(vec3 p,float R,float r){return length(vec2(length(p.xz)-R,p.y))-r;}
float cyl(vec3 p,float r,float h){vec2 d=abs(vec2(length(p.xz),p.y))-vec2(r,h);return min(max(d.x,d.y),0.)+length(max(d,0.));}
// ACES-ish filmic curve
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
