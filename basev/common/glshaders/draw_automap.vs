#version 110

varying vec4 Colour;

void main ()
{
	//	Transforming The Vertex
	gl_Position = (gl_ModelViewProjectionMatrix * gl_Vertex);

	//	Pass colour.
	Colour = gl_Color;
}
