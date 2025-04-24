from classes.Feature import *

class Extension:

	suffixes = [   \
		"AMD",     \
		"APPLE",   \
		"ARB",     \
		"ATI",     \
		"EXT",     \
		"GREMEDY", \
		"IBM",     \
		"IMG",     \
		"INGR",    \
		"INTEL",   \
		"KHR",     \
		"NV",      \
		"OES",     \
		"OML",     \
		"PGI",     \
		"QCOM",    \
		"SGIS",    \
		"SGIX",    \
		"VIV",     \
		"WEBGL",   \
		"WIN"]

	def __init__(self, xml, api):

		self.name      = xml.attrib["name"] 	 # e.g., GL_ARB_compute_variable_group_size
		self.supported = xml.attrib["supported"] # e.g., gl, gles2, gl|glcore
		self.incore    = None	# the lates feature by which all requires are in core

		self.api       = api if api in xml.attrib["supported"].split("|") else None

		self.reqEnums          = []
		self.reqCommands       = []

		self.reqEnumStrings    = []
		self.reqCommandStrings = []

		for require in xml.findall("require"):

			if "api" in require.attrib and require.attrib["api"] != api:
				continue

			for child in require:
				if   child.tag == "enum":
					self.reqEnumStrings.append(child.attrib["name"])
				elif child.tag == "command":
					self.reqCommandStrings.append(child.attrib["name"])

	def __str__(self):

		return "Extension (%s)" % (self.name)


	def __lt__(self, other):

		return self.name < other.name


	# currently there are extensions without requirements tagged as (none)
	def isNone(self):

		return len(self.enums) == 0 and len(self.commands) == 0


def suffixFreeEnumName(enum):

	i = enum.name.rfind("_")
	if i > 0 and enum.name[i + 1:] in Extension.suffixes:
		return enum.name[:i]

	return enum.name


def parseExtensions(xml, features, api):

	extensions = []
	for E in xml.iter("extensions"):

		# only parse extension if 
		# (1) supported apis includes requested api

		for extension in E.findall("extension"):

			# enforce constraint (1)
			if "supported" in extension.attrib and \
			   api not in extension.attrib["supported"].split("|"):
				continue

			extensions.append(Extension(extension, api))

			# ToDo: there might be none requiring extensions

	return sorted(extensions)


def resolveExtensions(extensions, enumsByName, commandsByName, features, api):

	for ext in extensions:

		ext.reqEnums = [enumsByName[e] for e in ext.reqEnumStrings]
		ext.reqCommands = [commandsByName[c] for c in ext.reqCommandStrings]

	if api != "gl":
		return

	# resolve incore
	# ToDo: several automated approaches were tested, none of which 
	# resulted in correct, specification matching mapping. 

	featuresByNumber = dict([(feature.number, feature) for feature in features])
	extensionsByName = dict([(extension.name, extension) for extension in extensions])

	mapping = {
		"3.0" : [
			"GL_ARB_map_buffer_range",
			# "GL_APPLE_flush_buffer_range"
			"GL_ARB_vertex_array_object",
			# "GL_APPLE_vertex_array_object",
			"GL_ARB_color_buffer_float",
			"GL_ARB_depth_buffer_float",
			"GL_ARB_framebuffer_object",
			"GL_ARB_framebuffer_sRGB",
			"GL_ARB_half_float_pixel",
			"GL_ARB_texture_compression_rgtc",
			"GL_ARB_texture_float",
			"GL_ARB_texture_rg",
			"GL_EXT_draw_buffers2",
			"GL_EXT_framebuffer_blit",
			"GL_EXT_framebuffer_multisample",
			"GL_EXT_gpu_shader4",
			"GL_EXT_packed_depth_stencil",
			"GL_EXT_packed_float",
			"GL_EXT_texture_array",
			"GL_EXT_texture_integer",
			"GL_EXT_texture_shared_exponent",
			# "GL_EXT_transform_feedback",
			"GL_NV_transform_feedback",
			"GL_NV_conditional_render",
			"GL_NV_half_float",
			# "GL_ARB_half_float_vertex" # ?
			],
		"3.1" : [
			"GL_ARB_copy_buffer",
			"GL_ARB_draw_instanced",
			"GL_ARB_texture_buffer_object",
			"GL_ARB_texture_rectangle",
			"GL_ARB_texture_snorm",
			# "GL_EXT_texture_snorm", # ?
			"GL_ARB_uniform_buffer_object",
			"GL_NV_primitive_restart"
			],
		"3.2": [
			"GL_ARB_depth_clamp",
			"GL_ARB_draw_elements_base_vertex",
			"GL_ARB_fragment_coord_conventions",
			"GL_ARB_geometry_shader4",
			"GL_ARB_provoking_vertex",
			"GL_ARB_seamless_cube_map",
			"GL_ARB_sync",
			"GL_ARB_texture_multisample",
			"GL_ARB_vertex_array_bgra",
			"GL_EXT_vertex_array_bgra" # ?
			],
		"3.3": [
			"GL_ARB_blend_func_extended",
			"GL_ARB_explicit_attrib_location",
			"GL_ARB_instanced_arrays",
			"GL_ARB_occlusion_query2",
			"GL_ARB_sampler_objects",
			"GL_ARB_texture_rgb10_a2ui",
			"GL_ARB_texture_swizzle",
			"GL_ARB_timer_query",
			"GL_ARB_vertex_type_2_10_10_10_rev"
			],
		"4.0": [
			"GL_ARB_draw_buffers_blend",
			"GL_ARB_draw_indirect",
			"GL_ARB_gpu_shader5",
			"GL_ARB_gpu_shader_fp64",
			"GL_ARB_sample_shading",
			"GL_ARB_shader_subroutine",
			"GL_ARB_tessellation_shader",
			"GL_ARB_texture_buffer_object_rgb32",
			"GL_ARB_texture_cube_map_array",
			"GL_ARB_texture_gather",
			"GL_ARB_texture_query_lod",
			"GL_ARB_transform_feedback2",
			"GL_ARB_transform_feedback3"
			],
		"4.1": [
			"GL_ARB_ES2_compatibility",
			"GL_ARB_get_program_binary",
			"GL_ARB_separate_shader_objects",
			"GL_ARB_shader_precision",
			"GL_ARB_vertex_attrib_64bit",
			"GL_ARB_viewport_array"
			],
		"4.2": [
			"GL_ARB_base_instance",
			"GL_ARB_compressed_texture_pixel_storage",
			"GL_ARB_conservative_depth",
			"GL_ARB_internalformat_query",
			"GL_ARB_map_buffer_alignment",
			"GL_ARB_robustness",
			"GL_ARB_shader_atomic_counters",
			"GL_ARB_shader_image_load_store",
			"GL_ARB_shading_language_420pack",
			"GL_ARB_texture_compression_bptc",
			"GL_ARB_texture_storage",
			"GL_ARB_transform_feedback_instanced"
			],
		"4.3": [
			"GL_ARB_arrays_of_arrays",
			"GL_ARB_clear_buffer_object",
			"GL_ARB_compute_shader",
			"GL_ARB_copy_image",
			"GL_ARB_debug_group",
			"GL_ARB_debug_label",
			#"GL_EXT_debug_label", # ?
			"GL_ARB_debug_output",
			#"GL_KHR_debug_output", # ?
			"GL_ARB_debug_output2",
			"GL_ARB_ES3_compatibility",
			"GL_ARB_explicit_uniform_location",
			"GL_ARB_framebuffer_no_attachments",
			"GL_ARB_internalformat_query2",
			"GL_ARB_invalidate_subdata",
			"GL_ARB_multi_draw_indirect",
			"GL_ARB_program_interface_query",
			"GL_ARB_shader_image_size",
			"GL_ARB_shader_storage_buffer_object",
			"GL_ARB_stencil_texturing",
			"GL_ARB_texture_buffer_range",
			"GL_ARB_texture_storage_multisample",
			"GL_ARB_texture_view",
			"GL_ARB_vertex_attrib_binding",
			"GL_ARB_texture_query_levels",
			"GL_ARB_fragment_layer_viewport",
			"GL_ARB_robust_buffer_access_behavior"
			],
		"4.4": [
			"GL_ARB_buffer_storage",
			"GL_ARB_clear_texture",
			"GL_ARB_enhanced_layouts",
			"GL_ARB_multi_bind",
			"GL_ARB_query_buffer_object",
			"GL_ARB_texture_mirror_clamp_to_edge",
			"GL_ARB_texture_stencil8",
			"GL_ARB_vertex_type_10f_11f_11f_rev"
			],
		"4.5": [
			"GL_ARB_clip_control",
			"GL_ARB_cull_distance",
			"GL_ARB_ES3_1_compatibility",
			"GL_ARB_conditional_render_inverted",
			"GL_KHR_context_flush_control",
			"GL_ARB_derivative_control",
			"GL_ARB_direct_state_access",
			"GL_ARB_get_texture_sub_image",
			"GL_KHR_robustness",
			"GL_ARB_shader_texture_image_samples",
			"GL_ARB_texture_barrier"
			]
		}

	for version, exts in mapping.items():
		for ext in exts:
			if ext in extensionsByName:
				extensionsByName[ext].incore = featuresByNumber[version]
