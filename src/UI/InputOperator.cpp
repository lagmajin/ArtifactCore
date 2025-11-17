module;

module Input.Operator;




namespace ArtifactCore
{
 class InputOperator::Impl {
 private:

 public:
  Impl();
  ~Impl();
  bool active = false;
};

 InputOperator::Impl::Impl()
 {
 }

 InputOperator::Impl::~Impl()
 {
 }

 InputOperator::InputOperator():impl_(new Impl())
 {

 }

 InputOperator::~InputOperator()
 {
  delete impl_;
 }

};