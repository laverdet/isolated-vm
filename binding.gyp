{
	'target_defaults': {
		'default_configuration': 'Release',
		'configurations': {
			'Release': {
				'xcode_settings': {
					'GCC_OPTIMIZATION_LEVEL': '3',
				},
			},
		},
	},
	'targets': [
		{
			'target_name': 'isolated_vm',
			'cflags_cc': [ '-std=c++14', '-g', '-Wno-unknown-pragmas' ],
			'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
			'xcode_settings': {
				'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
				'GCC_ENABLE_CPP_RTTI': 'YES',
				'GCC_GENERATE_DEBUGGING_SYMBOLS': 'YES',
				'CLANG_CXX_LANGUAGE_STANDARD': 'c++14',
			},
			'msvs_settings': {
				'VCCLCompilerTool': {
					'AdditionalOptions': [ '/GR' ],
					'ExceptionHandling': '1',
					'RuntimeTypeInfo': 'true',
				},
			},
			'msvs_disabled_warnings': [
				4101, # Unreferenced local (msvc fires these for ignored exception)
				4068, # Unknown pragma
			],
			'conditions': [
				[ 'OS == "win"',
					{ 'defines': [ 'NOMINMAX' ] },
					{ 'sources': [ 'src/array_buffer_shim.cc' ] },
				],
			],
			'sources': [
				'src/isolate/class_handle.cc',
				'src/isolate/environment.cc',
				'src/isolate/holder.cc',
				'src/isolate/three_phase_task.cc',
# 'src/inspector.cc',
				'src/context_handle.cc',
				'src/external_copy.cc',
				'src/external_copy_handle.cc',
				'src/isolate.cc',
				'src/isolate_handle.cc',
				'src/lib_handle.cc',
				'src/native_module_handle.cc',
				'src/reference_handle.cc',
				'src/script_handle.cc',
				'src/transferable.cc',
			],
		},
	],
}
