in vec4 pc;
in float pdist;
in float psz;
in float pline;
out vec4 o;
layout(binding=1) uniform sampler2D D;
void main(){
 vec2 c=pline>.5?vec2(0):gl_PointCoord*2.-1.;
 float r=dot(c,c);
 if(r>1.)discard;
 float sd=texelFetch(D,ivec2(gl_FragCoord.xy),0).x;
 float vis=smoothstep(0.,.05*pdist,sd-pdist);   // hidden behind surfaces, softly
 float a=psz<2.?exp(-r*3.):mix(exp(-r*3.),1.-smoothstep(.75,1.,r),.8);   // bokeh discs when defocused
 o=vec4(pc.rgb*a*vis,0);
}
