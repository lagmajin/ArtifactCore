# ECS + Archive Serialization Reference (from Wicked Engine)

> Source: https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiECS.h
>        https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiArchive.h
> Target: C-ARC-5 Deterministic Snapshot, C-LYR-3 Layer Normalization

## ECS Architecture

```cpp
// Entity = グローバル一意な uint64_t（アトミックインクリメント）
using Entity = uint64_t;
inline static constexpr Entity INVALID_ENTITY = 0;

inline Entity CreateEntity() {
    static std::atomic<Entity> next{ INVALID_ENTITY + 1 };
    return next.fetch_add(1);
}
```

### ComponentManager<T>

```cpp
template<typename T>
class ComponentManager : public ComponentManager_Interface {
    // Dense array of components (cache-friendly)
    wi::vector<T> components;
    // Entity → index lookup (unordered_map)
    wi::unordered_map<Entity, size_t> entity_to_index;
    // Free list for reuse
    wi::vector<size_t> free_indices;

public:
    T& Create(Entity entity) { ... }  // 新規 or 再利用
    void Remove(Entity entity) { ... } // free list に戻す
    bool Contains(Entity entity) const;
    T* Get(Entity entity);            // nullable
    T& operator[](size_t index);      // dense array access (for iteration)
    size_t GetCount() const;

    void Copy(const ComponentManager& other);   // deep copy
    void Merge(ComponentManager& other);        // otherの内容を取り込む
    void Serialize(Archive& archive, EntitySerializer& seri);
    void Component_Serialize(Entity entity, Archive& archive, ...);
};
```

### ComponentLibrary

```cpp
class ComponentLibrary {
    struct Entry {
        uint64_t version;                          // 前方互換バージョン
        ComponentManager_Interface* component_manager;
    };
    wi::unordered_map<std::string, Entry> entries;

public:
    template<typename T>
    ComponentManager<T>& Register(const std::string& name, uint64_t version = 0);

    // 全コンポーネントを一括シリアライズ
    void Serialize(Archive& archive, EntitySerializer& seri);
    // 特定 Entity の全コンポーネントをシリアライズ
    void Entity_Serialize(Entity entity, Archive& archive, EntitySerializer& seri);
};
```

### Scene

```cpp
struct Scene {
    ComponentLibrary componentLibrary;

    ComponentManager<NameComponent>&      names       = componentLibrary.Register<NameComponent>("names");
    ComponentManager<TransformComponent>& transforms  = componentLibrary.Register<TransformComponent>("transforms");
    ComponentManager<MaterialComponent>&  materials   = componentLibrary.Register<MaterialComponent>("materials", 11);
    // ... 21 component managers total
};
```

## Archive Serialization

```cpp
class Archive {
    struct Header {
        uint64_t version = 0;    // アーカイブのバージョン
        union Properties {
            struct {
                uint64_t thumbnail_data_size : 32;
                uint64_t compressed : 1;
                uint64_t reserved : 31;
            } bits;
            uint64_t raw = 0;
        } properties;
    };

    bool readMode;
    wi::vector<uint8_t> DATA;
    const uint8_t* data_ptr;  // memory-mapped or DATA.data()

public:
    Archive();                           // write mode, empty
    Archive(const string& file, bool readMode = true); // file-based
    Archive(const uint8_t* data, size_t size); // memory-mapped read
    ~Archive() { Close(); }              // auto-save if write mode + filename set

    bool IsOpen() const;
    bool IsReadMode() const;
    uint64_t GetVersion() const;
    size_t GetPos() const;

    void SetReadModeAndResetPos(bool isReadMode);
    void Close();  // writes file if needed

    // Type-safe operator<< / operator>> for all primitive types
    // ALL integer types are stored as 64-bit for cross-platform compatibility
    Archive& operator<<(int64_t data);
    Archive& operator>>(int64_t& data);
    Archive& operator<<(const string& data) {
        uint64_t len = data.length();
        (*this) << len;
        for (auto c : data) (*this) << c;
    }
    Archive& operator>>(string& data) {
        uint64_t len; (*this) >> len;
        data.resize(len);
        for (size_t i = 0; i < len; ++i) (*this) >> data[i];
    }
    template<typename T>
    Archive& operator>>(vector<T>& data) {
        size_t count; (*this) >> count;
        data.resize(count);
        for (auto& x : data) (*this) >> x;
    }

    // Forward-compatibility: skip unknown data
    size_t WriteUnknownJumpPosition();
    void PatchUnknownJumpPosition(size_t offset);
};
```

## Key Design Decisions

1. **全整数型が 64bit 保存**: `int`, `uint32_t`, `size_t` すべてが `int64_t`/`uint64_t` で
   保存される。32/64bit プラットフォーム間の互換性を保証。
2. **前方互換スキップ**: `WriteUnknownJumpPosition()` + `PatchUnknownJumpPosition()` で
   不明なコンポーネントのデータを読み飛ばし可能。
3. **Memory-mapped read**: ファイル全体を mmap してコピーせずに読み取り可能。
4. **2-pass deserialization**: 読み取り時はまず全バージョンを収集し、
   その後データを読み取る。コンポーネント間の相互参照に対応。
5. **Compression**: zlib 圧縮対応（オプション）

## ArtifactCore Adaptations

1. **C-ARC-5 Deterministic Snapshot**: Archive の `operator<<`/`operator>>` パターンを
   採用し、render/playback/export/AI の入力 state を snapshot として固定。
2. **Component 分割**: Layer の責務を `TransformComponent` + `BlendComponent` +
   `EffectComponent` に分割する方向性の参照。
3. **ComponentLibrary**: 全コンポーネントの一括シリアライズ＋バージョン管理の決定版。
4. **前方互換**: 新バージョンで追加されたプロパティを古いコードが読み飛ばせる仕組み。