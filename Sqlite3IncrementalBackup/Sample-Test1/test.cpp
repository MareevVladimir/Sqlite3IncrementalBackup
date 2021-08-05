#include "pch.h"
#include "..\Sqlite3IncrementalBackup\api.h"
#include <sqlite3.h>

TEST(Backup1, Backup) {
	sqlite3* db = nullptr;
	const char* filename = ".\\test.sqlite";
	int rc = sqlite3_open(filename, &db);
	char* msg = nullptr;
	
	sqlite3_exec(db, "CREATE TABLE test (col1 NUM, col2 TEXT)", NULL, NULL, NULL);
	for (int i = 0; i < 100; ++i)
		sqlite3_exec(db, "INSERT INTO test (col1, col2) VALUES (1, 'test')", NULL, NULL, NULL);

	sqlite3_inc_bkp::backup(db, ".\\", "test", &msg);
	
	EXPECT_EQ(1, 1);
	EXPECT_TRUE(true);
}