int i = 12;
float f = 1.2;
string s = "Test";

table テスト表 = {
	{ indexed int Id, string 名前, float value, table sub { int a, int b} },
	{ 0,    "テスト", 1.2, { { 5, 6 } } },
	{ 1001, "Test",   2.1, { { 7, 8 } } },
};

list テストリスト = { 1, 2, 3 };

tree テスト木 = {
	ノードA = {
		リスト = (list) { 0, 4 },
		子ノード = {
			葉ノード = "テスト",
		},
	},
	ノードB = {
		リスト = (list) { 8, 9 },
		子ノード = {
			葉ノード = "TEST",
		},
	},
	表ノード = (table) {
		{ indexed int idx, int a, int b },
		{ 0, 4, 8 },
		{ 1, -8, -16 },
	},
};
