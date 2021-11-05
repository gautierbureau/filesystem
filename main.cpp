#include <iostream>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <optional>
#include <memory>

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

template <class T>
class Counter {
 public:
 public:
  Counter() { ++getCount(); }
  Counter(const Counter& counter) : Counter{} {}
  Counter(Counter&& counter) noexcept : Counter{} {}
  ~Counter() { --getCount();  }
  static size_t getNbInstances() { return getCount(); }
 protected:
  static size_t& getCount() { static size_t count{0}; return count; }
};

class Folder;
class Shortcut;

class Element : private NonCopyable, public Counter<Element> {
 public:
  friend std::default_delete<const Element>;
  virtual Size getSize() const = 0;
  Element(const Element&) = delete;
  Element& operator= (const Element&) = delete;
  const Name getAbsoluteName() const;
  friend std::ostream& operator<< (std::ostream& out, const Element& element);
  friend Folder;
  friend Shortcut;

 protected:
  Element(const Name& name, const Folder* parent);
  virtual ~Element() = default;
  const Folder* getParent() const noexcept;

 private:
  // template method
  void output(std::ostream& out, unsigned int indentLevel) const noexcept;
  // hook 1
  virtual const std::string& getType() const noexcept = 0;
  // hook 2 : hook appele par la template method (le patron)
  virtual void onElementDisplayed(std::ostream& out, unsigned int indentLevel) const noexcept = 0;
  const Name name_;
  const Folder* const parent_ = nullptr;

  virtual const std::shared_ptr<const Element>& getSharedPtr() const noexcept;
};

class File final : public Element, public Counter<File> {
 public:
  friend Folder;
  virtual Size getSize() const override;
  using Counter<File>::getNbInstances;
 private:
  File(const Name& fileName, Size size, const Folder& parent);
  virtual ~File() = default ;
  const std::string& getType() const noexcept override;
  virtual void onElementDisplayed(std::ostream& out, unsigned int indentLevel) const noexcept override;
  const Size size_;
};

class Folder : public Element, public Counter<Folder> {
 public:
  // static Folder& getRoot();
  File& createFile(const Name& fileName, Size fileSize);
  Folder& createFolder(const Name& folderName);
  Shortcut& createShortcut(const Name& shortcutName, const Element& target);
  virtual Size getSize() const override;
  void removeElement(const Name& elementName);
  friend Element;
  using Counter<Folder>::getNbInstances;
 protected:
  virtual ~Folder();
  using Element::Element;
 private:
  using Key = Name;
//  explicit Folder(const Name& folderName);
  static Key keyFromName(const Name& name);

  const std::string& getType() const noexcept override;

  std::unordered_map<Key, std::shared_ptr<const Element>> children_;
  mutable std::optional<Size> computedSize_;

  const Key checkNameAvailability(const Name& elementName) const;

  virtual void onElementDisplayed(std::ostream& out, unsigned int indentLevel) const noexcept override;

  const std::shared_ptr<const Element>& getSharedPtrFromChild(const Name& childNname) const noexcept;

  template <class T, class... Args>
  T& createElement(const Name& elementName, Args... args) {
    const Key key{checkNameAvailability(elementName)};
    T* element{new T {elementName, args...}};
    children_[key].reset(element, std::default_delete<const Element>{});
    return *element;
  }

  void invalidateSize() const noexcept;
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
  const std::string& getType() const noexcept override;
  const std::shared_ptr<const Element>& getSharedPtr() const noexcept override;
  Size capacity_;
};

class Shortcut final : public Element, public Counter<Shortcut> {
 public:
  friend Folder;
  virtual Size getSize() const noexcept override;
  using Counter<Shortcut>::getNbInstances;
 private:
  Shortcut(const Name& fileName, const Element& target, const Folder& parent);
  virtual ~Shortcut() = default ;
  const std::string& getType() const noexcept override;
  void onElementDisplayed(std::ostream& out, unsigned int indentLevel) const noexcept override;
  std::weak_ptr<const Element> target_;
};

Element::Element(const Name& name, const Folder* parent) : name_{name}, parent_{parent} {
  if (name.empty())
    throw domain_error {"invalid name."};
}

const Name Element::getAbsoluteName() const {
  Name leftPart = parent_ ? parent_->getAbsoluteName() + "/" : "";
  return leftPart + name_;
}

const Folder* Element::getParent() const noexcept {
  return parent_;
}

//Folder& Folder::getRoot() {
//  static Folder root{"/", nullptr};
//  return root;
//}

//Folder::Folder(const Name& name) : Element { name } {
//}

const std::shared_ptr<const Element>& Element::getSharedPtr() const noexcept {
  return parent_->getSharedPtrFromChild(name_);
}

Folder::~Folder() {
}

Folder& Folder::createFolder(const Name& folderName) {
  return createElement<Folder>(folderName, this);
}

File& Folder::createFile(const Name& fileName, Size size) {
  if (size != 0)
    invalidateSize();
  return createElement<File>(fileName, size, std::cref(*this));
}

Shortcut& Folder::createShortcut(const Name& shortcutName, const Element& target) {
  return createElement<Shortcut>(shortcutName, std::cref(target), std::cref(*this));
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
  invalidateSize();
  children_.erase(keyFromName(elementName));
}

Size Folder::getSize() const {
  if (!computedSize_.has_value()) {
    Size size{0_bytes};
    for (const auto& [_, element] : children_)
      size += element->getSize();
    computedSize_ = size;
  }
  return computedSize_.value();
}

void Folder::invalidateSize() const noexcept {
  computedSize_.reset();
  if (getParent())
    getParent()->invalidateSize();
}

const std::shared_ptr<const Element>& Folder::getSharedPtrFromChild(const Name& childNname) const noexcept {
  return children_.at(keyFromName(childNname));
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
  static Partition instance{"/r1", 10'000_bytes};
  return instance;
}

void Partition::checkRemainingSize(Size desiredSize) const {
  if (capacity_ - getSize() < desiredSize)
    throw domain_error {"capacity overflow."};
}

const std::shared_ptr<const Element>& Partition::getSharedPtr() const noexcept {
//  struct PartitionDeleter {
//    void operator()(const Element* e) {}
//  };
//  static std::shared_ptr<const Element> sp { &getInstance(), PartitionDeleter{}};
  static std::shared_ptr<const Element> sp { &getInstance(), [](const Element* e) {}};
  return sp;
}

std::ostream& operator<< (std::ostream& out, const Element& element) {
  unsigned int indentLevel{0};
  element.output(out, indentLevel);
  return out;
}

void Element::output(std::ostream& out, unsigned int indentLevel) const noexcept {
  out << getType() << ": " << name_;
  onElementDisplayed(out, indentLevel);
}

const std::string& File::getType() const noexcept {
  static std::string type{"File"};
  return type;
}

const std::string& Folder::getType() const noexcept {
  static std::string type{"Folder"};
  return type;
}

const std::string& Partition::getType() const noexcept {
  static std::string type{"Partition"};
  return type;
}

void File::onElementDisplayed(std::ostream& out, unsigned int indentLevel) const noexcept {
}

void Folder::onElementDisplayed(std::ostream& out, unsigned int indentLevel) const noexcept {
  ++indentLevel;
  for (const auto& [_, element] : children_) {
    out << "\n" << std::string(indentLevel * 2, ' ');
    element->output(out, indentLevel);
  }
}

const std::string& Shortcut::getType() const noexcept {
  static std::string type{"Shortcut"};
  return type;
}

void Shortcut::onElementDisplayed(std::ostream& out, unsigned int indentLevel) const noexcept {
  const std::shared_ptr<const Element> sp {target_.lock()};
  cout << " --> " << (sp ? sp->getAbsoluteName() : "inexisting element");
}

Shortcut::Shortcut(const Name& fileName, const Element& target, const Folder& parent) : Element { fileName, &parent}, target_{target.getSharedPtr()} {
}

Size Shortcut::getSize() const noexcept {
  return 0;
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
    // r2.removeElement("f2");
    cout << r1.getSize() << " bytes\n";
    Folder& r3{ r2.createFolder("r3") };
    File& f3 = r1.createFile("f3", 899_bytes);
    File& f4 = r3.createFile("f4", 899_bytes);

    Shortcut& s1{ r2.createShortcut("s1", f1) };
    Shortcut& s2{ r2.createShortcut("s2", f2) };
    Shortcut& s3 {r2.createShortcut("shortcut on root", r1)};

    std::cout << f1.getAbsoluteName() << std::endl;
    // std::cout << f2.getAbsoluteName() << std::endl;
    std::cout << r1.getAbsoluteName() << std::endl;

    cout << r1 << std::endl;
    cout << f1 << std::endl;
    cout << r2 << std::endl;
    r2.removeElement("f2");
    cout << r2 << std::endl;

    cout << Element::getNbInstances() << " elements\n";
    cout << File::getNbInstances() << " files\n";
    cout << Shortcut::getNbInstances() << " shortcuts\n";
    cout << Folder::getNbInstances() << " folders\n";
  } catch (const std::exception& e) {
    cout << e.what() << std::endl;
  }

  return 0;
}
