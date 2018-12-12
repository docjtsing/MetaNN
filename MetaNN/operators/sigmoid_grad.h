#pragma once

#include <MetaNN/data/facilities/traits.h>
#include <MetaNN/evaluate/facilities/eval_plan.h>
#include <MetaNN/evaluate/facilities/eval_unit.h>
#include <MetaNN/operators/facilities/tags.h>
#include <MetaNN/operators/facilities/operator_frame.h>
#include <stdexcept>

namespace MetaNN
{
namespace OperSigmoidGrad::NSCaseGen
{
template <typename TGradHandle, typename TInputHandle, typename TOutputHandle, typename TDevice>
class EvalUnit : public BaseEvalUnit<TDevice>
{
public:
    EvalUnit(TGradHandle gradHandle, TInputHandle inputHandle, TOutputHandle outputHandle)
        : m_gradHandle(std::move(gradHandle))
        , m_inputHandle(std::move(inputHandle))
        , m_outputHandle(std::move(outputHandle))
    {}
    
    void Eval() override final
    {
        const auto& grad = m_gradHandle.Data();
        const auto& in = m_inputHandle.Data();
        assert(grad.Shape() == in.Shape());
        
        m_outputHandle.Allocate(grad.Shape());
        auto& out = m_outputHandle.MutableData();
        
        using ElementType = ElementTypePicker<decltype(out)>;
        
        const size_t count = in.Shape().Count();
        assert(count == out.Shape().Count());
        
        auto low_grad = LowerAccess(grad);
        ElementType* mem_grad = low_grad.MutableRawMemory();
        auto low_in = LowerAccess(in);
        ElementType* mem_in = low_in.MutableRawMemory();

        auto low_out = LowerAccess(out);
        ElementType* mem_out = low_out.MutableRawMemory();
                
        static_assert(std::is_same_v<TDevice, DeviceTags::CPU>, "Currently only CPU is supported");
        
        for (size_t i = 0; i < count; ++i)
        {
            mem_out[i] = mem_grad[i] * mem_in[i] * (1 - mem_in[i]);
        }
        m_outputHandle.SetEval();
    }
    
private:
    const TGradHandle m_gradHandle;
    const TInputHandle m_inputHandle;
    TOutputHandle m_outputHandle;
};

struct Calculator
{
    template <typename TCaseTail, typename TEvalRes, typename TOp>
    static void EvalRegister(TEvalRes& evalRes, const TOp& oper)
    {
        static_assert(std::is_same_v<TCaseTail, OperSeqContainer<>>,
                      "General case is not the last one");
                      
        using DeviceType = typename TEvalRes::DataType::DeviceType;

        const auto& data1 = oper.template GetOperand<0>();
        const auto& data2 = oper.template GetOperand<1>();
        auto handle1 = data1.EvalRegister();
        auto handle2 = data2.EvalRegister();
        
        auto outHandle = evalRes.Handle();
        using UnitType = EvalUnit<decltype(handle1), decltype(handle2), decltype(outHandle), DeviceType>;
        using GroupType = TrivalEvalGroup<UnitType>;

        const void* dataPtr = outHandle.DataPtr();
        std::vector<const void*> depVec{handle1.DataPtr(), handle2.DataPtr()};
        UnitType unit(std::move(handle1), std::move(handle2), std::move(outHandle));
        EvalPlan<DeviceType>::template Register<GroupType>(std::move(unit), dataPtr, depVec);
    }
};
}

template <>
struct OperSeq_<OpTags::SigmoidGrad>
{
    using type = OperSeqContainer<OperSigmoidGrad::NSCaseGen::Calculator>;
};

template <typename TGrad, typename TInput,
          typename = std::enable_if_t<IsValidOper<OpTags::SigmoidGrad, TGrad, TInput>>>
auto SigmoidGrad (TGrad&& p_grad, TInput&& p_input)
{
    if (p_grad.Shape() != p_input.Shape())
    {
        throw std::runtime_error("SigmoidGrad error: operands' shape mismatch.");
    }
    
    using rawOp1 = RemConstRef<TGrad>;
    using rawOp2 = RemConstRef<TInput>;
    using ResType = Operator<OpTags::SigmoidGrad, rawOp1, rawOp2>;
    return ResType(std::forward<TGrad>(p_grad), std::forward<TInput>(p_input));
}
}