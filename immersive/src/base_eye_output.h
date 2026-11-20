/* Current own-Base images are display encoded; legacy authored Live images
   retain their accepted linear passthrough path. No format/source VFX change. */
static inline ID3D11PixelShader* bp_eye_output_shader(bool owned,ID3D11PixelShader* authored,ID3D11PixelShader* display) {
 return owned?display:authored;
}
