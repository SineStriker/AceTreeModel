# AceTreeEntity

---

## 文档模型封装

### AceTreeEntity

`AceTreeEntity`是对`AceTreeItem`的封装，将纯数据的`item`包装为提供清晰的属性读写接口的`entity`。

例如，如果有一个`Json`描述的对象
```json
{
    "name": "Alice",
    "gender": "female",
    "age": 18,
    "talents": [
        {
            "type": "sports",
            "name": "basketball",
        },
        {
            "type": "linguist",
            "name": "english"
        }
    ]
}
```

可将其封装为
```c++
class StudentEntity: public AceTreeStandardEntity {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(Gender gender READ gender WRITE setGender NOTIFY genderChanged)
    Q_PROPERTY(int age READ age WRITE setAge NOTIFY ageChanged)
public:
    explicit StudentEntity(QObject *parent = nullptr);
    ~StudentEntity();

    enum Gender {
        Male,
        Female,
    };
    Q_ENUM(Gender)

public:
    QString name() const;
    void setName(const QString &name);

    Gender gender() const;
    void setGender(Gender gender);

    int age() const;
    void setAge(int age);

    TalentList *talents() const;

Q_SIGNALS:
    void nameChanged(const QString &name);
    void genderChanged(Gender gender);
    void ageChanged(int age);
}
```

+ 如果自己创建，则需要在`new`之后调用`initialize`进行初始化
+ 如果根据`AceTreeItem`生成，则需要调用`setup`进行装载