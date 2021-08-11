#include "backup.h"
#include <sqlite3.h>

namespace sqlite3_inc_bkp {
	
	std::string BackupV1::GetPageHashesCacheFilePath() const {
		return tools::FormatString::format("%s\\.%s.manifest", this->workspace.string().c_str(), this->name);
	}
	
	void BackupV1::BackupImpl(sqlite3* src) {
		std::string manifestFile = this->GetPageHashesCacheFilePath();
			
		this->hashes.clear();
		if (boost::filesystem::exists(manifestFile)) {
			ReadPageHashes(manifestFile.c_str());
		}	
		
		auto stmtRead = GetPageCursor(src);		
		std::size_t i = 1;

		if (!boost::filesystem::exists(this->GetBackupDbPath())) {
			std::ofstream fdbn(this->GetBackupDbPath(), std::ios::binary);
			fdbn.close();
		}
		//boost::filesystem::resize_file(this->GetBackupDbPath(), this->GetPageCount(src) * 4096);
		
		if (this->hashes.empty())
			this->hashes.push_back(0);

		std::ofstream fdb(this->GetBackupDbPath(), std::ios::in | std::ios::out | std::ios::binary);
		while (1) {
			int status = sqlite3_step(stmtRead);
			if (status == SQLITE_DONE) break;

			else if (status == SQLITE_ROW) {	
				const void* data = sqlite3_column_blob(stmtRead, 0);
				const std::size_t size = sqlite3_column_bytes(stmtRead, 0);
				
				hash_t inputHash = this->hashFunction(data, size);
		
				if (i >= this->hashes.size()) {					
					WritePage(i - 1, data, size, fdb);					
					this->hashes.push_back(inputHash);
				}
				else if (this->hashes.at(i) != (inputHash = this->hashFunction(data, size))) {					
					WritePage(i - 1, data, size, fdb);
					this->hashes.at(i) = inputHash;
				}
				++i;				
			}
			else
				throw BackupException(tools::FormatString::format("Error fetching data: %s", sqlite3_errstr(status)).c_str(), BackupException::Error::SelectPages);
		}		
		sqlite3_finalize(stmtRead);
		fdb.close();

		this->hashes.at(0) = this->hashFunction(reinterpret_cast<char *>(&this->hashes[1]), (this->hashes.size() - 1) * sizeof(hash_t));
		WritePageHashes();		
	}		
	
	void BackupV1::ReadImpl(sqlite3* dst) {
		//Check backup file
		if (!boost::filesystem::exists(this->GetBackupDbPath())) {
			throw BackupException(tools::FormatString::format("Backup file [%s] not exists", this->GetBackupDbPath().c_str()).c_str(), BackupException::Error::BackupInit);
		}
		sqlite3* bckp = nullptr;
		int rc = sqlite3_open_v2(this->GetBackupDbPath().c_str(), &bckp, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
		if (rc != SQLITE_OK) {
			throw BackupException(tools::FormatString::format("sqlite3 open error: (%d) %s", rc, sqlite3_errstr(rc)).c_str(), BackupException::Error::BackupInit);
		}
		
		IntegrityCheck(dst, bckp);
		
		//Read
		
		auto loadFromBackup = sqlite3_backup_init(
			dst, "main",
			bckp, "main");
		if (loadFromBackup == nullptr) {
			throw BackupException("sqlite3 backup init error", BackupException::Error::BackupInit);
		}
		
		rc = sqlite3_backup_step(loadFromBackup, -1);
		if (rc != SQLITE_DONE) {
			throw BackupException(tools::FormatString::format("sqlite3 backup error: (%d) %s", rc, sqlite3_errstr(rc)).c_str(), BackupException::Error::BackupInit);
		}
		sqlite3_backup_finish(loadFromBackup);
	}

	void BackupV1::IntegrityCheck(sqlite3 *dst, sqlite3 *src) {
		if (!boost::filesystem::exists(this->GetPageHashesCacheFilePath())) {
			throw BackupException(tools::FormatString::format("Integrity file [%s] not exists", this->GetBackupDbPath().c_str()).c_str(), BackupException::Error::IntegrityCheck);
		}
		this->ReadPageHashes(this->GetPageHashesCacheFilePath().c_str());
		hash_t currdatahash = this->hashFunction(reinterpret_cast<char*>(&this->hashes[1]), (this->hashes.size() - 1) * sizeof(hash_t));
		if (this->hashes.at(0) != currdatahash) {
			throw BackupException(tools::FormatString::format("Integrity file [%s] corrupted", this->GetPageHashesCacheFilePath().c_str()).c_str(), BackupException::Error::IntegrityCheck);
		}
		CheckDbIntegrity(src);
	}

	void BackupV1::CheckDbIntegrity(sqlite3* src) {
		auto stmtRead = this->GetPageCursor(src, 1);
		int status = sqlite3_step(stmtRead);		
		hash_t inputHash = 0;
		if (status == SQLITE_ROW) {
			const void* data = sqlite3_column_blob(stmtRead, 0);
			const std::size_t size = sqlite3_column_bytes(stmtRead, 0);

			inputHash = this->hashFunction(data, size);
		}
		else
			throw BackupException(tools::FormatString::format("Error fetching data: %s", sqlite3_errstr(status)).c_str(), BackupException::Error::IntegrityCheck);			
		
		sqlite3_finalize(stmtRead);	
		
		if (inputHash != this->hashes.at(1)) {
			throw BackupException("Sqlite3 database check integrity failed", BackupException::Error::IntegrityCheck);
		}
	}

	void BackupV1::ClearImpl() {
		this->hashes.clear();
		std::remove(this->GetPageHashesCacheFilePath().c_str());
		std::remove(this->GetBackupDbPath().c_str());
	}

	void BackupV1::WritePageHashes() const {		
		std::ofstream fManifest(this->GetPageHashesCacheFilePath(), std::ios::binary);
		std::for_each(this->hashes.begin(), this->hashes.end(), [&fManifest](hash_t sum) {
			fManifest.write(reinterpret_cast<const char*>(&sum), sizeof(sum));
		});		
	}
	
	void BackupV1::WritePage(std::size_t i, const void* data, std::size_t size, std::ofstream &fdb) {
		auto path = this->GetBackupDbPath();		
		if (!fdb.is_open())
			throw BackupException("Backup database file is not open", BackupException::Error::BackupInit);
		fdb.seekp(i * size);
		fdb.write(reinterpret_cast<const char*>(data), size);		
	}
	
	void BackupV1::ReadPageHashes(const char *path) {
		std::ifstream fManifest(path, std::ios::binary);
		
		this->hashes.clear();
		this->hashes.reserve(fManifest.tellg()/sizeof(hash_t));		
		fManifest.seekg(0);
		while(1) {
			hash_t sum = 0;
			fManifest.read(reinterpret_cast<char*>(&sum), sizeof(sum));
			if (fManifest.eof())
				break;
			this->hashes.push_back(sum);
		}
		
	}
	
	sqlite3_stmt* BackupV1::GetPageCursor(sqlite3 *db, int limit) const {
		sqlite3_stmt* stmt = nullptr;		
		const std::string query( limit == -1 ? std::string("SELECT data FROM sqlite_dbpage('main')") : tools::FormatString::format("SELECT data FROM sqlite_dbpage('main') ORDER BY pgno LIMIT %d", limit));
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

}// namespace sqlite3_inc_bkp