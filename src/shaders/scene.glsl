in vec2 q;
layout(location=0) out vec4 o;
layout(location=1) out float od;
void main(){o=vec4(q,.5+.5*sin(T),1);od=1e3;}
