uniform int			FogType;
uniform vec4		FogColour;
uniform float		FogDensity;
uniform float		FogStart;
uniform float		FogEnd;

void main()
{
	float z = gl_FragCoord.z / gl_FragCoord.w;
	const float LOG2 = 1.442695;
	float FogFactor;
	if (FogType == 3)
	{
		FogFactor = exp2(-FogDensity * FogDensity * z * z * LOG2);
	}
	else if (FogType == 2)
	{
		FogFactor = exp2(-FogDensity * z * LOG2);
	}
	else
	{
		FogFactor = (FogEnd - z) / (FogEnd - FogStart);
	}
	FogFactor = clamp(1.0 - FogFactor, 0.0, 1.0);
	gl_FragColor = vec4(FogColour.xyz, FogFactor);
}
