#version 330 core

in vec2 UV;

out vec4 color;

uniform sampler2D mainTexture;
uniform sampler2D lightTexture;
uniform vec4 ambientColor;
uniform vec2 resolution;

void main()
{	
	vec4 diffuseColor = texture(mainTexture, UV);
	vec2 lightCoord = (gl_FragCoord.xy / resolution.xy);
	vec4 light = texture(lightTexture, lightCoord);
	vec3 ambient = (ambientColor.rgb * ambientColor.a) + light.rgb;
	vec3 final = diffuseColor.rgb * ambient;
	color = vec4(final, diffuseColor.a);
}
