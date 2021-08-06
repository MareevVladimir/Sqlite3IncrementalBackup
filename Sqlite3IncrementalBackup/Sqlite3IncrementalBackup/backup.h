#pragma once

#include <string>
#include <stdio.h>

#include <boost/filesystem.hpp>
#include "exception.h"

struct sqlite3_stmt;
struct sqlite3;
namespace sqlite3_inc_bkp {
	using hash_t = uint64_t;
	using hash_func = std::function<hash_t(const void*, std::size_t)>;

	/// <summary>
	/// Interface class of incremental backup implemention
	/// </summary>
	class IBackup
	{
	public:
		enum class Version {
			V1
		};
		
		static std::unique_ptr <IBackup> Create(Version v, const char* path, const char* name, hash_func f);
		virtual void Write(sqlite3* db) = 0;
		
		virtual ~IBackup() {}
	};

	template <typename T>
	class Backup : public IBackup {
	public:
		Backup<T>(const char* path, const char* name, hash_func func) : workspace(path), name(name), hashFunction(func) {
			CreateWorkspaceIfNotExists();
		}
		
		void Write(sqlite3* db) override {
			auto pThis = static_cast<T*>(this);
			pThis->BackupImpl(db);
		}		
	private:
		void CreateWorkspaceIfNotExists() {
			if (boost::filesystem::exists(workspace) && boost::filesystem::is_directory(workspace)) {
				return;
			}
			try {
				if (!boost::filesystem::create_directories(workspace)) {
					throw BackupException(tools::FormatString::format("Could not create backup cache directory %s", workspace.c_str()).c_str());
				}
			}
			catch (const std::exception& e) {
				throw std::runtime_error(tools::FormatString::format("Could not create backup cache directory %s : %s", workspace.c_str(), e.what()).c_str());
			}
		}

	protected:
		boost::filesystem::path workspace;
		std::string name;
		hash_func hashFunction;
	};		

	/// <summary>
	/// Version 1 implementation of incremental backup
	/// </summary>
	class BackupV1 : public Backup<BackupV1> {
	public:		
		BackupV1(const char* path, const char* name, hash_func func) : Backup<BackupV1>(path, name, func) {}
	//Implementation backup method
		void BackupImpl(sqlite3* db);			
	private:
	//Reading data for backup
		sqlite3_stmt* GetPageCursor(sqlite3* db) const;
		std::size_t GetPageCount(sqlite3* db) const;		
		
	private:
	//Caching hashes on disk
		std::string GetPageHashesCacheFilePath() const;
		void WritePageHashes() const;
		void ReadPageHashes(const char *path);
		
	private:
	//Writing backup
		void WritePage(std::size_t i, const void* data, std::size_t size, std::ofstream &f);
		sqlite3* GetBackupDb() const;		
		std::string GetBackupDbPath() const;
	
	private:
		std::vector<hash_t> hashes;
	};
}//namespace sqlite3_inc_bkp