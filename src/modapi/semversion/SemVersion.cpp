#include <mc/deps/core/sem_ver/SemVersion.h>
#include <mc/deps/core/string/StaticOptimizedString.h>

// Definition for symbol not exported in LeviLamina 26.20.0 server

SemVersion::SemVersion()
: SemVersionBase(
      0,
      0,
      0,
      ::Bedrock::StaticOptimizedString("", ::Bedrock::StaticOptimizedString::StorageType::Static),
      ::Bedrock::StaticOptimizedString("", ::Bedrock::StaticOptimizedString::StorageType::Static)
  ) {}
