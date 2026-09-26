// ---- post: bloom pyramid, depth of field, grade, title ------------------------
in vec2 q;
out vec4 o;
layout(binding=0) uniform sampler2D A;
layout(binding=1) uniform sampler2D B;
layout(binding=2) uniform sampler2D X;
layout(binding=3) uniform sampler2D D;
#define FADE U[62].x
#define TITLE U[62].y
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
void main(){
#ifdef DOWN
 // 13-tap downsample (soft, stable)
 vec2 s=1./vec2(textureSize(A,0));
 vec4 c=texture(A,q)*.5+(texture(A,q+s*vec2(1,1))+texture(A,q+s*vec2(-1,1))+texture(A,q+s*vec2(1,-1))+texture(A,q+s*vec2(-1,-1)))*.125;
 if(any(isnan(c)))c=vec4(0);
 o=min(c,vec4(64));
#endif
#ifdef UP
 vec2 s=1./vec2(textureSize(A,0));
 o=(texture(A,q)*4.+texture(A,q+vec2(s.x,0))+texture(A,q-vec2(s.x,0))+texture(A,q+vec2(0,s.y))+texture(A,q-vec2(0,s.y)))*.125*.9;
#endif
#ifdef FINAL
 vec2 res=vec2(textureSize(A,0)),p=q;
 // at speed the edges of the frame pull everything out of it (U[66].x)
 float sp=U[66].x,rr=length((q-.5)*vec2(1.6,1));
 p=.5+(p-.5)*(1.-sp*.1*rr*rr);
 // depth of field: gather over a golden-angle disc, weights from each sample's circle of confusion
 float fd=U[3].x,ap=U[3].y*res.y;
 float d0=texture(D,p).x;
 float c0=min(ap*abs(d0-fd)/max(d0,1e-3),24.);
 vec3 acc=texture(A,p).rgb;float ws=1.;
 float R=min(24.,ap*.9+1.);
 if(ap>.01)for(int i=0;i<40;i++){
  float r=sqrt((float(i)+.5)/40.)*R,a=float(i)*2.39996;
  vec2 uv=p+vec2(cos(a),sin(a))*r/res;
  float ds=texture(D,uv).x,cs=min(ap*abs(ds-fd)/max(ds,1e-3),24.);
  if(ds>d0)cs=min(cs,c0);
  float w=smoothstep(r-1.5,r,cs);
  acc+=texture(A,uv).rgb*w;ws+=w;
 }
 if(sp>0.)for(int i=1;i<9;i++){acc+=texture(A,.5+(p-.5)*(1.-float(i)*.009*sp*rr*rr)).rgb;ws+=1.;}   // radial streaking
 vec3 c=acc/ws;
 // chromatic fringe towards the frame edges (optical, subtle; stronger at speed)
 vec2 e=(p-.5)*(.004+.012*sp)*length(p-.5);
 c.r=mix(c.r,texture(A,p+e).r,.5);c.b=mix(c.b,texture(A,p-e).b,.5);
 c+=texture(B,p).rgb*.07;
 // anamorphic lens: the brightest lights draw thin horizontal streaks, slightly blue
 vec3 st=vec3(0);
 for(int i=-14;i<=14;i++)st+=max(texture(B,vec2(p.x+float(i)*.011,p.y)).rgb-1.6,0.)*exp(-abs(float(i))*.2);
 c+=st*vec3(.3,.5,1.)*.025;
 // exposure / grade
 c=aces(c*.9);
 float g=U[3].w;
 vec3 cool=vec3(.97,1.,1.03),astral=vec3(1.02,.98,1.06);
 c=pow(c,vec3(1.04))*mix(cool,astral,clamp(g-1.,0.,1.));
 c*=1.-.28*pow(length((q-.5)*vec2(1.3,1.)),2.2);
 // title
 if(TITLE>0.){
  vec2 tq=vec2((q.x-.5)/.74+.5,(q.y-.5)/.217+.5);
  float H=float(textureSize(X,0).y);
  if(tq.x>0.&&tq.x<1.&&tq.y>0.&&tq.y<1.)c+=vec3(.9,.95,1.)*texture(X,vec2(tq.x,1.-(1.-tq.y)*256./H)).r*TITLE;
 }
 // the lyrics: white letters outlined in the singer's colour, forming in a luminous sweep, leaving as grains
 for(int k=0;k<2;k++){vec4 L=U[64+k];
  vec2 tq=vec2((q.x-.5)/.952+.5,(q.y-.034-.08*smoothstep(0.,.45,L.z))/.07);
  if(L.w<.5||tq.x<0.||tq.x>1.||tq.y<0.||tq.y>1.)continue;
  float H=float(textureSize(X,0).y);
  vec2 au=vec2(tq.x,1.-(256.+(L.x+1.-tq.y)*64.)/H),px=vec2(1./2048.,1./H);
  float m=texture(X,au).r,od=m,gl=0.;
  for(int i=0;i<12;i++){float a=float(i)*.5236;vec2 o=vec2(cos(a),sin(a));od=max(od,texture(X,au+o*px*2.8).r);gl+=texture(X,au+o*px*8.).r;}
  gl/=12.;
  vec3 oc=L.w<1.5?vec3(.25,1.,.5):L.w<2.5?vec3(1.,.3,.25):vec3(.8,.8,.83);
  float n=fract(sin(dot(floor(au/px/2.),vec2(127.1,311.7)))*43758.5453);
  float fr=.12+L.y*.9,ap=smoothstep(fr,fr-.08,tq.x+(n-.5)*.06);
  float gd=n*.9+.05,dp=smoothstep(L.z-.08,L.z+.04,gd);
  float glow=(exp(-pow((tq.x-fr+.05)/.04,2.))*step(L.y,.999)+exp(-pow((gd-L.z)/.05,2.))*step(.001,L.z))*od;
  float a=ap*dp;
  c=mix(c,mix(oc*.9,vec3(1,.98,.95),m),clamp(od*1.3,0.,1.)*a*.95)+oc*(gl*.3*a+glow*1.4*ap);
 }
 // grain
 c+=(fract(sin(dot(q*res+fract(U[0].x*7.1)*93.,vec2(12.9898,78.233)))*43758.55)-.5)*.018;
 o=vec4(c*(1.-FADE),1);
#endif
}
