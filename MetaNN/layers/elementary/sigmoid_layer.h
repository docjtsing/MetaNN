#pragma once

#include <MetaNN/layers/facilities/common_io.h>
#include <MetaNN/layers/facilities/policies.h>
#include <MetaNN/layers/facilities/traits.h>
#include <MetaNN/policies/policy_operations.h>
#include <MetaNN/policies/policy_selector.h>
#include <stack>

namespace MetaNN
{
    struct LayerInput;
    struct LayerOutput;
    
    template <typename TInputs, typename TGrads, typename TPolicies>
    class SigmoidLayer
    {
        static_assert(IsPolicyContainer<TPolicies>);
        using CurLayerPolicy = PlainPolicy<TPolicies>;

    public:
        static constexpr bool IsFeedbackOutput = PolicySelect<GradPolicy, CurLayerPolicy>::IsFeedbackOutput;
        static constexpr bool IsUpdate = false;

        using InputMap = TInputs;
        using GradMap = FillGradMap<TGrads, LayerOutput>;
        
    private:
        using TLayerInputFP = typename InputMap::template Find<LayerInput>;
        using TLayerOutputBP = typename GradMap::template Find<LayerOutput>;

        auto FeedForwardCal(const TLayerInputFP& val)
        {
            return Sigmoid(val);
        }

    public:
        SigmoidLayer(std::string name)
            : m_name(std::move(name))
        {}
        
        template <typename TIn>
        auto FeedForward(TIn&& p_in)
        {
            auto val = LayerTraits::PickItemFromCont<InputMap, LayerInput>(std::forward<TIn>(p_in));
            auto res = FeedForwardCal(val);

            if constexpr (IsFeedbackOutput)
            {
                m_inputShape.Push(val.Shape());
                m_outputShape.Push(res.Shape());
                m_data.push(res);
            }
            return LayerOutputCont<SigmoidLayer>().template Set<LayerOutput>(std::move(res));
        }

        template <typename TGrad>
        auto FeedBackward(TGrad&& p_grad)
        {
            if constexpr (IsFeedbackOutput)
            {
                if (m_data.empty())
                {
                    throw std::runtime_error("Cannot feed back in SigmoidLayer");
                }
                auto grad = LayerTraits::PickItemFromCont<GradMap, LayerOutput>(std::forward<TGrad>(p_grad));
                m_outputShape.CheckAndPop(grad.Shape());
                
                auto input = m_data.top();
                m_data.pop();
                
                auto res = SigmoidGrad(std::move(grad), std::move(input));
                m_inputShape.CheckAndPop(res.Shape());
                return LayerInputCont<SigmoidLayer>().template Set<LayerInput>(std::move(res));
            }
            else
            {
                return LayerInputCont<SigmoidLayer>();
            }
        }

        void NeutralInvariant() const
        {
            if constexpr(IsFeedbackOutput)
            {
                if (!m_data.empty())
                {
                    throw std::runtime_error("NeutralInvariant Fail!");
                }
                m_inputShape.AssertEmpty();
                m_outputShape.AssertEmpty();
            }
        }

    private:
        std::string m_name;
        using TempDataType = RemConstRef<std::invoke_result_t<decltype(&SigmoidLayer::FeedForwardCal), SigmoidLayer, TLayerInputFP>>;
        LayerTraits::LayerInternalBuf<TempDataType, IsFeedbackOutput> m_data;

        LayerTraits::ShapeChecker<ShapeType<TLayerInputFP>,  IsFeedbackOutput> m_inputShape;
        LayerTraits::ShapeChecker<ShapeType<TLayerOutputBP>, IsFeedbackOutput> m_outputShape;
    };
}