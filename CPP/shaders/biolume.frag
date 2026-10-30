#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform vec2 u_resolution;
uniform float u_time, u_bass, u_treble, u_pulse;
uniform vec4 u_cells[8]; // xy: centers; z: radius; w: energy
uniform vec4 u_flies[48];
uniform vec3 u_cellColors[8]; // xy: world position, z: flutter phase
uniform vec4 u_glows[16]; // xy: capture position, w: remaining lifetime

float softUnion(float a,float b,float k){
    float h=max(k-abs(a-b),0.0)/k;
    return min(a,b)-h*h*k*.25;
}
float noise(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void main(){
    float unit=min(u_resolution.x,u_resolution.y);
    vec2 p=(gl_FragCoord.xy-.5*u_resolution)/unit;
    float t=u_time;
    vec3 background=mix(vec3(.012,.006,.045),vec3(.045,.008,.065),smoothstep(-.55,.55,p.y));
    background+=.008*vec3(.2,.4,.8)*sin(p.y*13.+t*.2);
    float sdf=5.0,core=0.0,halo=0.0;
    for(int i=0;i<8;i++){
        vec4 c=u_cells[i];
        vec2 v=p-c.xy;
        float a=atan(v.y,v.x);
        float deform=.009*sin(7.*a+t*.9+float(i)*1.7)+.006*sin(11.*a-t*.55+float(i));
        float radius=c.z*(1.+.045*u_bass)+deform;
        float d=length(v)-radius;
        sdf=softUnion(sdf,d,.018);
        core+=c.w*exp(-dot(v,v)/(radius*radius*.70));
        halo+=.04*exp(-abs(d)*22.);
    }
    float membrane=exp(-abs(sdf)*110.);
    float body=1.-smoothstep(-.025,.025,sdf);
    float translucent=body*(.25+.18*sin(8.*p.x+3.*p.y+t*.28));
    vec3 color=background;
    vec3 pigments=vec3(0.0);
    float pigmentWeight=0.0;
    for(int i=0;i<8;i++){
        vec2 d=p-u_cells[i].xy;
        float w=exp(-dot(d,d)/max(.0001,u_cells[i].z*u_cells[i].z*.72));
        pigments+=u_cellColors[i]*w;
        pigmentWeight+=w;
    }
    pigments/=max(.0001,pigmentWeight);
    color+=pigments*translucent*.48;
    color+=pigments*membrane*.55;
    color+=vec3(.04,.12,.18)*halo;
    // Green luminescence from swallowed flies stays INSIDE the transparent membrane.
    float bio=core*.22;
    for(int i=0;i<16;i++){
        vec4 g=u_glows[i];
        float age=1.-g.w;
        vec2 d=p-g.xy;
        float spread=.014+age*.13;
        bio+=g.w*.9*exp(-dot(d,d)/(spread*spread));
        // A soft wave travelling over the connected cell interior.
        float ring=abs(length(d)-age*.21);
        bio+=g.w*.13*exp(-ring*ring/.00007);
    }
    color+=body*bio*vec3(.12,1.75,.30);
    color+=membrane*bio*.32*vec3(.23,.95,.38);
    // Fireflies: warm cores, neon-green emission and soft atmospheric halos.
    vec3 flyLight=vec3(0.);
    for(int i=0;i<48;i++){
        vec4 f=u_flies[i];
        vec2 d=p-f.xy;
        float r2=dot(d,d);
        float flicker=.70+.30*sin(t*9.+f.z*5.);
        float glow=exp(-r2/.00020)*flicker;
        float dotlight=exp(-r2/.000010)*flicker;
        flyLight+=glow*vec3(.025,.37,.055)+dotlight*vec3(.32,1.0,.40);
    }
    color+=flyLight;
    // Faint, animated iridescence over the membrane.
    color+=membrane*.06*vec3(.5+.5*sin(t*.7+p.y*28.),.22,.8);
    color=1.-exp(-color*1.1); // gentle tone mapping
    color=pow(max(color,vec3(0.)),vec3(.90));
    fragColor=vec4(color,1.);
}
