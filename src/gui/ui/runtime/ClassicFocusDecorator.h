#pragma once

#include <RmlUi/Core.h>
#include <RmlUi/Core/Decorator.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/PropertyDefinition.h>
#include <RmlUi/Core/RenderManager.h>
#include <cmath>

namespace ingnomia::ui
{
// Original untextured geometry. The cue occupies no layout space and leaves
// the button bevel, selected-tab seam and default-action outline independent.
class ClassicFocusDecorator final : public Rml::Decorator
{
public:
    explicit ClassicFocusDecorator(Rml::Colourb color) : color_(color) {}
    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* element, Rml::BoxArea) const override
    {
        const float dp = element->GetContext()->GetDensityIndependentPixelRatio();
        const float dot = std::max(1.f, std::round(dp));
        const float inset = std::round(4.f * dp);
        const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
        const float right = std::floor(size.x - inset - dot);
        const float bottom = std::floor(size.y - inset - dot);
        if(right < inset || bottom < inset) return 0;
        Rml::Mesh mesh;
        const auto color = color_.ToPremultiplied(element->GetComputedValues().opacity());
        const auto square = [&](float x, float y) {
            const int first = static_cast<int>(mesh.vertices.size());
            mesh.vertices.push_back({{x,y},color,{}});
            mesh.vertices.push_back({{x+dot,y},color,{}});
            mesh.vertices.push_back({{x+dot,y+dot},color,{}});
            mesh.vertices.push_back({{x,y+dot},color,{}});
            for(int index : {0,1,2,0,2,3}) mesh.indices.push_back(first+index);
        };
        for(float x = inset; x <= right; x += 2.f * dot) { square(x,inset); square(x,bottom); }
        for(float y = inset + 2.f * dot; y < bottom; y += 2.f * dot) { square(inset,y); square(right,y); }
        auto* geometry = new Rml::Geometry(element->GetContext()->GetRenderManager().MakeGeometry(std::move(mesh)));
        return reinterpret_cast<Rml::DecoratorDataHandle>(geometry);
    }
    void ReleaseElementData(Rml::DecoratorDataHandle data) const override
    {
        delete reinterpret_cast<Rml::Geometry*>(data);
    }
    void RenderElement(Rml::Element* element, Rml::DecoratorDataHandle data) const override
    {
        reinterpret_cast<Rml::Geometry*>(data)->Render(element->GetAbsoluteOffset(Rml::BoxArea::Border));
    }
private:
    Rml::Colourb color_;
};

class ClassicFocusInstancer final : public Rml::DecoratorInstancer
{
public:
    ClassicFocusInstancer()
    {
        color_ = RegisterProperty("color", "#000000").AddParser("color").GetId();
        RegisterShorthand("decorator", "color", Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String&, const Rml::PropertyDictionary& properties,
        const Rml::DecoratorInstancerInterface&) override
    {
        return Rml::MakeShared<ClassicFocusDecorator>(properties.GetProperty(color_)->Get<Rml::Colourb>());
    }
private:
    Rml::PropertyId color_;
};
}
