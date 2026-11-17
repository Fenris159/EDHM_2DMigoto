// Input signature:
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// TEXCOORD                 0   x           0     NONE   float   x
// Output signature:
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   x           0   TARGET   float   x
ps_5_0
dcl_input_ps linear v0.x
dcl_output o0.x
mov o0.x, v0.x
ret
