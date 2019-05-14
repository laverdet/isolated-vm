{
	'target_defaults': {
		'default_configuration': 'Release',
		'configurations': {
			'Common': {
				'abstract': 1,
				'cflags_cc': [ '-std=c++14', '-g', '-Wno-unknown-pragmas' ],
				'cflags_cc!': [ '-fno-exceptions' ],
				'xcode_settings': {
					'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
					'GCC_GENERATE_DEBUGGING_SYMBOLS': 'YES',
					'CLANG_CXX_LANGUAGE_STANDARD': 'c++14',
					'MACOSX_DEPLOYMENT_TARGET': '10.9',
				},
				'msvs_settings': {
					'VCCLCompilerTool': {
						'AdditionalOptions': [ '/GR' ],
						'ExceptionHandling': '1',
					},
				},
				'msvs_disabled_warnings': [
					4101, # Unreferenced local (msvc fires these for ignored exception)
					4068, # Unknown pragma
				],
				'conditions': [
					[ 'OS == "win"', { 'defines': [ 'NOMINMAX' ] } ],
				],
			},
			'Release': {
				'inherit_from': [ 'Common' ],
				'cflags': [ '-Wno-deprecated-declarations' ],
				'xcode_settings': {
					'GCC_OPTIMIZATION_LEVEL': '3',
					'OTHER_CFLAGS': [ '-Wno-deprecated-declarations' ],
				},
			},
			'Debug': {
				'inherit_from': [ 'Common' ],
				'defines': [ 'V8_IMMINENT_DEPRECATION_WARNINGS' ],
			},
		},
	},
	'targets': [
		{
			'target_name': 'isolated_vm',
			'cflags_cc!': [ '-fno-rtti' ],
			'xcode_settings': {
				'GCC_ENABLE_CPP_RTTI': 'YES',
			},
			'msvs_settings': {
				'VCCLCompilerTool': {
					'RuntimeTypeInfo': 'true',
				},
			},
			'conditions': [
				[ 'OS == "linux"', { 'defines': [ 'USE_CLOCK_THREAD_CPUTIME_ID' ] } ],
			],
			'sources': [
				'src/isolate/allocator.cc',
				'src/isolate/class_handle.cc',
				'src/isolate/environment.cc',
				'src/isolate/holder.cc',
				'src/isolate/inspector.cc',
				'src/isolate/stack_trace.cc',
				'src/isolate/three_phase_task.cc',
				'src/context_handle.cc',
				'src/external_copy.cc',
				'src/external_copy_handle.cc',
				'src/isolate.cc',
				'src/isolate_handle.cc',
				'src/lib_handle.cc',
				'src/native_module_handle.cc',
				'src/reference_handle.cc',
				'src/script_handle.cc',
				'src/module_handle.cc',
				'src/session_handle.cc',
				'src/transferable.cc',
			],
			'dependencies': [ 'nortti' ],
		},
		{
			'target_name': 'nortti',
			'type': 'static_library',
			'sources': [ 'src/external_copy_nortti.cc' ],
		},
	],
}
