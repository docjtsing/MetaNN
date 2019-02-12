#pragma once
#include <MetaNN/layers/facilities/common_io.h>
#include <MetaNN/layers/facilities/policies.h>
#include <MetaNN/layers/facilities/traits.h>
#include <MetaNN/policies/policy_operations.h>
#include <MetaNN/policies/policy_selector.h>

namespace MetaNN
{
    struct InterpolateLayerWeight1; struct InterpolateLayerWeight2; struct InterpolateLayerLambda;
    struct LayerOutput;

    template <typename TInputs, typename TGrads, typename TPolicies>
    class InterpolateLayer
    {
        static_assert(IsPolicyContainer<TPolicies>);
        using CurLayerPolicy = PlainPolicy<TPolicies>;

    public:
        static constexpr bool IsFeedbackOutput = PolicySelect<GradPolicy, CurLayerPolicy>::IsFeedbackOutput;
        static constexpr bool IsUpdate = false;
        
        using InputMap = TInputs;
        using GradMap = FillGradMap<TGrads, LayerOutput>;
        
    private:
        using TInterpolateLayerWeight1FP = typename InputMap::template Find<InterpolateLayerWeight1>;
        using TInterpolateLayerWeight2FP = typename InputMap::template Find<InterpolateLayerWeight2>;
        using TInterpolateLayerLambdaFP  = typename InputMap::template Find<InterpolateLayerLambda>;
        using TLayerOutputBP             = typename GradMap::template Find<LayerOutput>;

        auto FeedForwardCal(const TInterpolateLayerWeight1FP& val1,
                            const TInterpolateLayerWeight2FP& val2,
                            const TInterpolateLayerLambdaFP& lambda)
        {
            auto proShape = LayerTraits::ShapePromote(val1.Shape(), val2.Shape(), lambda.Shape());
            return Interpolate(Duplicate(std::move(val1), proShape),
                               Duplicate(std::move(val2), proShape),
                               Duplicate(std::move(lambda), proShape));
        }
    public:
        InterpolateLayer(std::string name)
            : m_name(std::move(name))
        {}
        
        template <typename TIn>
        auto FeedForward(TIn&& p_in)
        {
            auto input1 = LayerTraits::PickItemFromCont<InputMap, InterpolateLayerWeight1>(std::forward<TIn>(p_in));
            auto input2 = LayerTraits::PickItemFromCont<InputMap, InterpolateLayerWeight2>(std::forward<TIn>(p_in));
            auto lambda = LayerTraits::PickItemFromCont<InputMap, InterpolateLayerLambda>(std::forward<TIn>(p_in));
            auto res = FeedForwardCal(input1, input2, lambda);

            if constexpr (IsFeedbackOutput)
            {
                m_weight1Shape.Push(input1.Shape());
                m_weight2Shape.Push(input2.Shape());
                m_lambdaShape.Push(lambda.Shape());
                m_outputShape.Push(res.Shape());

                m_input1Stack.push(std::move(input1));
                m_input2Stack.push(std::move(input2));
                m_lambdaStack.push(std::move(lambda));
            }

            return LayerOutputCont<InterpolateLayer>().template Set<LayerOutput>(std::move(res));
        }

        template <typename TGrad>
        auto FeedBackward(TGrad&& p_grad)
        {
            if constexpr (IsFeedbackOutput)
            {
                if ((m_input1Stack.empty()) || (m_input2Stack.empty()) || (m_lambdaStack.empty()))
                {
                    throw std::runtime_error("Cannot do FeedBackward for InterpolateLayer");
                }
                auto grad = LayerTraits::PickItemFromCont<GradMap, LayerOutput>(std::forward<TGrad>(p_grad));
                m_outputShape.CheckAndPop(grad.Shape());
                
                auto curLambda = m_lambdaStack.top();
                auto curInput1 = m_input1Stack.top();
                auto curInput2 = m_input2Stack.top();
                m_lambdaStack.pop();
                m_input1Stack.pop();
                m_input2Stack.pop();

                auto res2 = grad * Duplicate(1 - curLambda, grad.Shape());
                auto res1 = grad * Duplicate(curLambda, grad.Shape());
                auto resLambda = grad * (Duplicate(curInput1, grad.Shape()) - Duplicate(curInput2, grad.Shape()));
                
                auto out1 = Collapse(std::move(res1), curInput1.Shape());
                auto out2 = Collapse(std::move(res2), curInput2.Shape());
                auto outLambda = Collapse(std::move(resLambda), curLambda.Shape());
                
                m_weight1Shape.CheckAndPop(out1.Shape());
                m_weight2Shape.CheckAndPop(out2.Shape());
                m_lambdaShape.CheckAndPop(outLambda.Shape());

                return LayerInputCont<InterpolateLayer>()
                    .template Set<InterpolateLayerWeight1>(std::move(out1))
                    .template Set<InterpolateLayerWeight2>(std::move(out2))
                    .template Set<InterpolateLayerLambda>(std::move(outLambda));
            }
            else
            {
                return LayerInputCont<InterpolateLayer>();
            }
        }

        void NeutralInvariant()
        {
            if constexpr(IsFeedbackOutput)
            {
                if ((!m_input1Stack.empty()) || (!m_input2Stack.empty()) || (!m_lambdaStack.empty()))
                {
                    throw std::runtime_error("NeutralInvariant Fail!");
                }
                m_weight1Shape.AssertEmpty();
                m_weight2Shape.AssertEmpty();
                m_lambdaShape.AssertEmpty();
                m_outputShape.AssertEmpty();
            }
        }
    private:
        std::string m_name;
        LayerTraits::LayerInternalBuf<TInterpolateLayerWeight1FP, IsFeedbackOutput> m_input1Stack;
        LayerTraits::LayerInternalBuf<TInterpolateLayerWeight2FP, IsFeedbackOutput> m_input2Stack;
        LayerTraits::LayerInternalBuf<TInterpolateLayerLambdaFP,  IsFeedbackOutput> m_lambdaStack;

        LayerTraits::ShapeChecker<ShapeType<TInterpolateLayerWeight1FP>,  IsFeedbackOutput> m_weight1Shape;
        LayerTraits::ShapeChecker<ShapeType<TInterpolateLayerWeight2FP>,  IsFeedbackOutput> m_weight2Shape;
        LayerTraits::ShapeChecker<ShapeType<TInterpolateLayerLambdaFP>,   IsFeedbackOutput> m_lambdaShape;
        LayerTraits::ShapeChecker<ShapeType<TLayerOutputBP>, IsFeedbackOutput> m_outputShape;
    };
}