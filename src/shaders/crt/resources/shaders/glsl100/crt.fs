#version 100

precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

#define	TEX2D(c) texture2D(texture0, c)
#define FIX(c) max(abs(c), 1e-5);
#define PI 3.141592654

float Gamma = 2.2;
float MonitorGamma = 2.4;
float Overscan = 0.99;

uniform float Brightness;
uniform float ScanlineIntensity;
uniform bool Curvature;
uniform float CurvatureRadius;
uniform float CornerSize;
uniform float Cornersmooth;
uniform bool Border;

uniform vec2 resolution;

vec4 scanlineWeights(float distance, vec4 color) {
	vec4 wid = 2.0 * pow(color, vec4(4.0)) + 6.0;
	vec4 weights = vec4(distance * 1.32);        
	return 0.51 * exp(-pow(weights * sqrt(2.0 / wid), wid)) / (0.48 + 0.06 * wid);
}

float corner(vec2 coord) {
    coord = (coord - 0.5) * vec2(Overscan, Overscan) + 0.5;
	coord = min(coord, 1.0 - coord) * vec2(0.4,1.0);
	vec2 cdist = vec2(CornerSize/100.0);
	coord = (cdist - min(coord,cdist));
	float dist = sqrt(dot(coord,coord));
    return clamp((cdist.x-dist) * Cornersmooth, 0.0, 1.0); 
}

vec2 radialDistortion(vec2 coord) {
  vec2 cc = coord - vec2(0.5);
  float dist = dot(cc, cc) * CurvatureRadius;
  return coord + cc * (1.0 - dist) * dist;
}

void main() {
	vec2 one = 1.0 / resolution;

	vec2 _xy = (Curvature) ? radialDistortion(fragTexCoord) : fragTexCoord;

	vec2 ratio_scale = _xy * resolution - 0.5;
	vec2 uv_ratio = fract(ratio_scale);
	
	float cval = 1.0;
	if(Border) cval = corner(_xy);

	_xy = (floor(ratio_scale) + 0.5) / resolution;

	vec4 coeffs = PI * vec4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x);

	coeffs = FIX(coeffs);
	coeffs = 2.0 * sin(coeffs) * sin(coeffs / 2.0) / (coeffs * coeffs);

	coeffs /= dot(coeffs, vec4(1.0));

	vec4 col  = clamp(coeffs.x * TEX2D(_xy + vec2(-one.x, 0.0))   + coeffs.y * TEX2D(_xy)+ coeffs.z * TEX2D(_xy + vec2(one.x, 0.0)) + coeffs.w * TEX2D(_xy + vec2(2.0 * one.x, 0.0)),   0.0, 1.0);
	vec4 col2 = clamp(coeffs.x * TEX2D(_xy + vec2(-one.x, one.y)) + coeffs.y * TEX2D(_xy + vec2(0.0, one.y)) + coeffs.z * TEX2D(_xy + one)+ coeffs.w * TEX2D(_xy + vec2(2.0 * one.x, one.y)), 0.0, 1.0);

	vec4 rgba = texture2D(texture0, _xy);
	vec4 intensity;
	
	if(fract(gl_FragCoord.y * (0.5 * 1.5 / 3.0)) > 0.5) intensity = vec4(0);
	else intensity = smoothstep(0.2, 0.5, rgba) + normalize(rgba);

	vec4 weights  = scanlineWeights(uv_ratio.y, col);
	vec4 weights2 = scanlineWeights(1.0 - uv_ratio.y, col2);
	
	vec3 mul_res = intensity.rgb + (col * weights + col2 * weights2).rgb * cval;
	
	float mod_factor = fragTexCoord.x * resolution.x;

	vec3 dotMaskWeights = mix( vec3(1.0, 0.7, 1.0), vec3(0.7, 1.0, 0.7), floor(mod(mod_factor, ScanlineIntensity)) );
	mul_res *= dotMaskWeights * Brightness;

	if (Border) {
		mul_res = pow(abs(mul_res), vec3(1.0 / (2.0 * Gamma - MonitorGamma))) * vec3(cval);
	} else {
		mul_res = pow(abs(mul_res), vec3(1.0 / ( 2.0 * Gamma - MonitorGamma)));
	}

	gl_FragColor = vec4(mul_res, 1.0);
}
