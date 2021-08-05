#include "backup.h"

namespace sqlite3_inc_bkp {
	
	std::string BackupV1::GetManifestFilePath() const {
		return tools::FormatString::format("%s\\.%s.manifest", this->workspace.string().c_str(), this->name);
	}
	
	void BackupV1::BackupImpl(sqlite3* src) {
		std::string manifestFile = this->GetManifestFilePath();
			
		this->hashes.clear();
		if (boost::filesystem::exists(manifestFile)) {
			ReadPageHashes(manifestFile.c_str());
		}	
		
		auto stmtRead = GetPageCursor(src);		
		std::size_t i = 0;
		if (!boost::filesystem::exists(this->GetBackupDbPath())) {
			std::ofstream fdbn(this->GetBackupDbPath(), std::ios::binary);
			fdbn.close();
		}
		//boost::filesystem::resize_file(this->GetBackupDbPath(), this->GetPageCount(src) * 4096);
		
		std::ofstream fdb(this->GetBackupDbPath(), std::ios::in | std::ios::out | std::ios::binary);
		while (1) {
			int status = sqlite3_step(stmtRead);
			if (status == SQLITE_DONE) break;

			else if (status == SQLITE_ROW) {	
				const void* data = sqlite3_column_blob(stmtRead, 0);
				const std::size_t size = sqlite3_column_bytes(stmtRead, 0);
				
				hash_t inputHash = this->hashFunction(data, size);
		
				if (i >= this->hashes.size()) {					
					WritePage(i, data, size, fdb);					
					this->hashes.push_back(inputHash);
				}
				else if (this->hashes.at(i) != (inputHash = this->hashFunction(data, size))) {					
					WritePage(i, data, size, fdb);
					this->hashes.at(i) = inputHash;
				}
				++i;				
			}
			else
				throw BackupException(tools::FormatString::format("Error fetching data: %s", sqlite3_errstr(status)).c_str(), BackupException::Error::SelectPages);
		}
		
		sqlite3_finalize(stmtRead);
		fdb.close();
		WriteHashes();
	}		
	
	void BackupV1::WriteHashes() const {		
		std::ofstream fManifest(this->GetManifestFilePath(), std::ios::binary);
		std::for_each(this->hashes.begin(), this->hashes.end(), [&fManifest](hash_t sum) {
			fManifest.write(reinterpret_cast<const char*>(&sum), sizeof(sum));
		});		
	}

	
	void BackupV1::WritePage(std::size_t i, const void* data, std::size_t size, std::ofstream &fdb) {
		auto path = this->GetBackupDbPath();		
		if (!fdb.is_open())
			throw 1;
		fdb.seekp(i * size);
		fdb.write(reinterpret_cast<const char*>(data), size);		
	}

	
	void BackupV1::ReadPageHashes(const char *path) {
		std::ifstream fManifest(path, std::ios::binary);
		
		this->hashes.clear();
		this->hashes.reserve(fManifest.tellg()/sizeof(hash_t));		
		fManifest.seekg(0);
		while (!fManifest.eof()) {
			hash_t sum = 0;
			fManifest.read(reinterpret_cast<char *>(&sum), sizeof(sum));
			this->hashes.push_back(sum);
		}		
	}
	
	
	sqlite3_stmt* BackupV1::GetPageCursor(sqlite3 *db) const {
		sqlite3_stmt* stmt = nullptr;
		const std::string query("SELECT data FROM sqlite_dbpage('main')");
		int rc = sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
		if (rc != SQLITE_OK) {
			throw BackupException(tools::FormatString::format("Error preparing SQL query '%s' : %s", query.c_str(), sqlite3_errstr(rc)).c_str(), BackupException::Error::SelectPages);
		}
		return stmt;
	}

	std::size_t BackupV1::GetPageCount(sqlite3* db) const {
		sqlite3_stmt* stmt = nullptr;		
		const std::string query("SELECT pgno FROM sqlite_dbpage('main') ORDER BY pgno DESC LIMIT 1");
		int rc = sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
		if (rc != SQLITE_OK) {
			throw BackupException(tools::FormatString::format("Error preparing SQL query '%s' : %s", query.c_str(), sqlite3_errstr(rc)).c_str(), BackupException::Error::SelectPages);
		}
		sqlite3_step(stmt);
		std::size_t pageCount = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
		return pageCount;
	}
	
	sqlite3_stmt* BackupV1::GetWritePageCursor(const std::string &query, sqlite3 *db) const {			
		sqlite3_stmt* stmt = nullptr;		
		int rc = sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
		if (rc != SQLITE_OK) {
			throw BackupException(tools::FormatString::format("Error preparing SQL query '%s' : %s", query.c_str(), sqlite3_errstr(rc)).c_str(), BackupException::Error::SelectPages);
		}
		return stmt;
	}

	
	sqlite3* BackupV1::GetBackupDb() const {
		sqlite3* db = nullptr;
		int res = sqlite3_open(this->GetBackupDbPath().c_str(), &db);
		if (SQLITE_OK != res) {
			throw BackupException(tools::FormatString::format("Error opening sqlite3 db backup file %s : %s", this->GetBackupDbPath().c_str(), sqlite3_errstr(res)).c_str(), BackupException::Error::BackupInit);
		}
		return db;
	}

	
	std::string BackupV1::GetBackupDbPath() const {
		return tools::FormatString::format("%s%s_backup.sqlite", this->workspace.string().c_str(), this->name);
	}

	std::unique_ptr <IBackup> IBackup::Create(Version v, const char* path, const char* name, hash_func f) {
		switch (v) {
		case Version::V1:
		default:
			return std::make_unique<BackupV1>(path, name, f);
		}
	}

	std::default_delete<class sqlite3_inc_bkp::IBackup> deleter;
}// namespace sqlite3_inc_bkp