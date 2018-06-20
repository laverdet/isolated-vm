{
	'targets': [
		{
			'target_name': 'isolated-native-example',
			'include_dirs': [
				'<!(node -e "require(\'nan\')")',
				'<!(node -e "require(\'isolated-vm/include\')")',
			],
			'sources': [
				'example.cc',
			],
		},
	],
}
