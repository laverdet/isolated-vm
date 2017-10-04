{
	'target_defaults': {
		'default_configuration': 'Release',
		'configurations': {
			'Release': {
				'xcode_settings': {
					'GCC_OPTIMIZATION_LEVEL': '3',
					'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
				},
				'msvs_settings': {
					'VCCLCompilerTool': {
						'Optimization': 3,
						'FavorSizeOrSpeed': 1,
					},
				},
			},
		},
	},
	'targets': [
		{
			'target_name': 'isolated_vm',
			'cflags_cc': [ '-std=c++14' ],
			'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
			'xcode_settings': {
				'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
				'GCC_ENABLE_CPP_RTTI': 'YES',
				'CLANG_CXX_LANGUAGE_STANDARD': 'c++14',
			},
			'msvs_settings': {
				'VCCLCompilerTool': {
					'ExceptionHandling': '1',
					'RuntimeTypeInfo': 'true',
					'AdditionalOptions': [ '/GR' ],
				},
			},
			'msvs_disabled_warnings': [ 4068 ], # Unknown pragma
			'conditions': [
				[ 'OS == "win"',
					{ 'defines': [ 'NOMINMAX' ] },
					{ 'sources': [ 'src/array_buffer_shim.cc' ] },
				],
			],
			'sources': [
				'src/class_handle.cc',
				'src/external_copy.cc',
				'src/isolate.cc',
				'src/shareable_isolate.cc',
				'src/transferable.cc',
			],
		},
	],
}
