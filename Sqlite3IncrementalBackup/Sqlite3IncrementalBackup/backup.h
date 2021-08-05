#pragma once

#include <array>
#include <string>
#include <stdio.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <sqlite3.h>

#define MESSAGE_BUFFER_SIZE 0x100

namespace tools {
	struct FormatString {
	public:
		template <typename... Args>
		static std::string format(
			std::string formatString,
			Args&&... args)
		{
			boost::format f(std::move(formatString));
			return formatImpl(f, std::forward<Args>(args)...).str();
		}
	private:
		static boost::format& formatImpl(boost::format& f)
		{
			return f;
		}

		template <typename Head, typename... Tail>
		static boost::format& formatImpl(
			boost::format& f,
			Head const& head,
			Tail&&... tail)
		{
			return formatImpl(f % head, std::forward<Tail>(tail)...);
		}
	};
}

namespace sqlite3_inc_bkp {
	using hash_t = uint64_t;
	using hash_func = std::function<hash_t(const void*, std::size_t)>;

	class BackupException : public std::exception {
	public:
		enum class Error : std::size_t {
			Unknown,
			SelectPages,
			BackupInit
		};
	private:
		static constexpr const char* ErrorStr(Error code) {
			switch (code) {
			case Error::SelectPages:
				return "Failed get database page data";			
			case Error::BackupInit:
				return "Failed init backup file";
			case Error::Unknown:
			default:
				return "Unknown error";
			}
		}

		std::string ErrorFullStr(Error code, const char* message) {
			char msg[MESSAGE_BUFFER_SIZE];
			if (!strcmp(message, ""))
				std::snprintf(msg, MESSAGE_BUFFER_SIZE, "(%d) %s", static_cast<std::size_t>(code), ErrorStr(code));
			else
				std::snprintf(msg, MESSAGE_BUFFER_SIZE, "(%d) %s: %s", static_cast<std::size_t>(code), ErrorStr(code), message);

			return std::string(msg);
		}
	public:
		BackupException(const char* message, Error err = Error::Unknown) : _error(err), std::exception(ErrorFullStr(err, message).c_str()) {}

	private:
		Error _error;
	};

	class IBackup
	{
	public:
		enum class Version {
			V1
		};

		
		static std::unique_ptr <IBackup> Create(Version v, const char* path, const char* name, hash_func f);
		virtual void DoBackup(sqlite3* db) = 0;
		
		virtual ~IBackup() {}
	};

	template <typename T>
	class Backup : public IBackup {
	public:
		Backup<T>(const char* path, const char* name, hash_func func) : workspace(path), name(name), hashFunction(func) {
			CreateWorkspaceIfNotExists();
		}
		
		void DoBackup(sqlite3* db) override {
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
	
	class BackupV1 : public Backup<BackupV1> {
	public:		
		BackupV1(const char* path, const char* name, hash_func func) : Backup<BackupV1>(path, name, func) {}
		void BackupImpl(sqlite3* db);		
	private:
		sqlite3_stmt* GetPageCursor(sqlite3* db) const;
		void WritePage(std::size_t i, const void* data, std::size_t size, std::ofstream &f);
		void WriteHashes() const;
		void ReadPageHashes(const char *path);
		std::size_t GetPageCount(sqlite3* db) const;
		std::string GetManifestFilePath() const;
		sqlite3_stmt* GetWritePageCursor(const std::string &query, sqlite3* db) const;
		sqlite3* GetBackupDb() const;		
		std::string GetBackupDbPath() const;
	private:
		std::vector<hash_t> hashes;
	};
}//namespace sqlite3_inc_bkp