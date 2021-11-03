#include <iostream>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>

using std::string;
using std::cout;
using std::domain_error;

using Size = unsigned;
using Capacity = Size;
using Name = string;

inline Size operator"" _bytes (unsigned long long value) {
  return static_cast<Size>(value);
}

class NonCopyable {
 protected:
  NonCopyable() = default;

  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};

class Folder;

class Element : private NonCopyable {
 public:
  friend Folder;
  virtual Size getSize() const = 0;
  Element(const Element&) = delete;
  Element& operator= (const Element&) = delete;
  const Name getAbsoluteName() const;
 protected:
  Element(const Name& name, const Folder* parent);
  virtual ~Element() = default;
 private:
  const Name name_;
  const Folder* const parent_ = nullptr;
};

class File final : public Element {
 public:
  friend class Folder;
  virtual Size getSize() const override;
 private:
  File(const Name& fileName, Size size, const Folder& parent);
  virtual ~File() = default ;
  const Size size_;
};

class Folder : public Element {
 public:
  // static Folder& getRoot();
  File& createFile(const Name& fileName, Size fileSize);
  Folder& createFolder(const Name& folderName);
  virtual Size getSize() const override;
  void removeElement(const Name& elementName);
 protected:
  virtual ~Folder();
  using Element::Element;
 private:
  using Key = Name;
//  explicit Folder(const Name& folderName);
  static Key keyFromName(const Name& name);

  std::unordered_map<Key, const Element*> children_;

  const Key checkNameAvailability(const Name& elementName) const;
};

class Partition final : public Folder {
 public:
  static Partition& getInstance();
  using Folder::getSize;
  friend File;
 private:
  Partition(const Name& name, Capacity capacity);
  ~Partition() = default;
  void checkRemainingSize(Size desiredSize) const;
  Size capacity_;
};

Element::Element(const Name& name, const Folder* parent) : name_{name}, parent_{parent} {
  if (name.empty())
    throw domain_error {"invalid name."};
}

const Name Element::getAbsoluteName() const {
  Name leftPart = parent_ ? parent_->getAbsoluteName() + "/" : "";
  return leftPart + name_;
}

//Folder& Folder::getRoot() {
//  static Folder root{"/", nullptr};
//  return root;
//}

//Folder::Folder(const Name& name) : Element { name } {
//}

Folder::~Folder() {
  for (const auto& [_, element] : children_)
    delete element;
}

Folder& Folder::createFolder(const Name& folderName) {
  const Key key{checkNameAvailability(folderName)};
  Folder* folder{ new Folder {folderName, this} };
  children_.insert({key, folder});
  return *folder;
}

File& Folder::createFile(const Name& fileName, Size size) {
  const Key key{checkNameAvailability(fileName)};
  File* file{new File {fileName, size, *this}};
  children_.insert({key, file});
  return *file;
}

const Folder::Key Folder::checkNameAvailability(const Name& elementName) const {
  const Key key{keyFromName(elementName)};
  auto itor {children_.find(key) };
  if (itor != cend(children_))
    throw domain_error{ elementName + " already exists." };
  return key;
}

void Folder::removeElement(const Name& elementName) {
  auto itor {children_.find(keyFromName(elementName))};
  if (itor == cend(children_))
    throw std::domain_error{ elementName + " does not exist."};
  delete itor->second;
  children_.erase(keyFromName(elementName));
}

Size Folder::getSize() const {
  Size size{0_bytes};
  for (const auto& [_, element] : children_)
    size += element->getSize();
  return size;
}

File::File(const Name& fileName, Size size, const Folder& parent) : Element { fileName, &parent}, size_ { size } {
  Partition::getInstance().checkRemainingSize(size);
}

Size File::getSize() const {
  return size_;
}

Folder::Key Folder::keyFromName(const Name& name) {
  Key key{name};
  std::transform(cbegin(key), cend(key), begin(key), toupper);
  return key;
}

Partition::Partition(const Name &name, Capacity capacity) : Folder{name, nullptr}, capacity_(capacity) {
}

Partition& Partition::getInstance() {
  static Partition instance{"/", 10'000_bytes};
  return instance;
}

void Partition::checkRemainingSize(Size desiredSize) const {
  if (capacity_ - getSize() < desiredSize)
    throw domain_error {"capacity overflow."};
}

int main() {
  try {
    // Folder& r1{ Folder::getRoot() };
    Partition& r1{ Partition::getInstance() };
    Folder& r2{ r1.createFolder("r2") };
    // Folder& r2_bis{ r1.createFolder("R2") };
    File& f1 = r1.createFile("f1", 899_bytes);
    // File& f1_bis = r1.createFile("F1", 899_bytes);
    // File& f1_empty = r1.createFile("", 899_bytes);
    File& f2{ r2.createFile("f2", 1234_bytes) };
    // File& f3{ r2.createFile("f3", 12'340_bytes) };
    cout << r1.getSize() << " bytes\n";
    //r2.removeElement("f2");

    std::cout << f1.getAbsoluteName() << std::endl;
    std::cout << f2.getAbsoluteName() << std::endl;
    std::cout << r1.getAbsoluteName() << std::endl;
  } catch (const std::exception& e) {
    cout << e.what() << std::endl;
  }

  return 0;
}
