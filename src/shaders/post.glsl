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
 vec2 res=vec2(textureSize(A,0));
 // depth of field: gather over a golden-angle disc, weights from each sample's circle of confusion
 float fd=U[3].x,ap=U[3].y*res.y;
 float d0=texture(D,q).x;
 float c0=min(ap*abs(d0-fd)/max(d0,1e-3),24.);
 vec3 acc=texture(A,q).rgb;float ws=1.;
 float R=min(24.,ap*.9+1.);
 if(ap>.01)for(int i=0;i<40;i++){
  float r=sqrt((float(i)+.5)/40.)*R,a=float(i)*2.39996;
  vec2 uv=q+vec2(cos(a),sin(a))*r/res;
  float ds=texture(D,uv).x,cs=min(ap*abs(ds-fd)/max(ds,1e-3),24.);
  if(ds>d0)cs=min(cs,c0);
  float w=smoothstep(r-1.5,r,cs);
  acc+=texture(A,uv).rgb*w;ws+=w;
 }
 vec3 c=acc/ws;
 // chromatic fringe towards the frame edges (optical, subtle)
 vec2 e=(q-.5)*.004*length(q-.5);
 c.r=mix(c.r,texture(A,q+e).r,.5);c.b=mix(c.b,texture(A,q-e).b,.5);
 c+=texture(B,q).rgb*.07;
 // exposure / grade
 c=aces(c*.9);
 float g=U[3].w;
 vec3 cool=vec3(.97,1.,1.03),astral=vec3(1.02,.98,1.06);
 c=pow(c,vec3(1.04))*mix(cool,astral,clamp(g-1.,0.,1.));
 c*=1.-.28*pow(length((q-.5)*vec2(1.3,1.)),2.2);
 // title
 if(TITLE>0.){
  vec2 tq=vec2((q.x-.5)*1.6+.5,(q.y-.5)*1.6*816./256.*.42+.5);
  if(tq.x>0.&&tq.x<1.&&tq.y>0.&&tq.y<1.)c+=vec3(.9,.95,1.)*texture(X,tq).r*TITLE;
 }
 // grain
 c+=(fract(sin(dot(q*res+fract(U[0].x*7.1)*93.,vec2(12.9898,78.233)))*43758.55)-.5)*.018;
 o=vec4(c*(1.-FADE),1);
#endif
}
