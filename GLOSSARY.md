# EDHM runtime language

This repository preserves EDHM's configuration behavior while preparing for
separate rendering hosts.

## Language

**IniParams**: Numbered rows of four values that EDHM rules and shader
replacements use to share configuration and scene information.

**IniParams publication**: Making the current IniParams values available to the
commands and shaders that consume them at a required execution point.

**ShaderOverride**: A rule selecting a shader's use and the EDHM behavior applied
there.

**TextureOverride**: A rule selecting resources by identity or matching
properties, with numeric filter values and optional commands.

**Preset**: A named set of value changes and optional commands, with transitions
that can apply changes over time.
