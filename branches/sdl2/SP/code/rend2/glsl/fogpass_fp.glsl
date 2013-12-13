uniform vec4  u_Color;

varying float var_Scale;

void main()
{
	gl_FragColor = u_Color;
#if defined(USE_WOLF_FOG_LINEAR) || defined(USE_WOLF_FOG_EXPONENTIAL)
	gl_FragColor.a *= var_Scale;
#else
	gl_FragColor.a = sqrt(clamp(var_Scale, 0.0, 1.0));
#endif
}
