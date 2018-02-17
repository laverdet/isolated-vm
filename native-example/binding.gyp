{
	'targets': [
		{
			'target_name': 'isolated-native-example',
			'include_dirs': [
				'<!(node -e "require(\'nan\')")',
			],
			'conditions': [
				[ 'OS == "win"',
					{ 'defines': [ 'IVM_DLLEXPORT=__declspec(dllexport)' ] },
					{ 'defines': [ 'IVM_DLLEXPORT=' ] },
				],
			],
			'sources': [
				'example.cc',
			],
		},
	],
}
