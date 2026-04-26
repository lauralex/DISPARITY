#pragma once

namespace Disparity
{
    class Application;
    class Renderer;
    class TimeStep;

    class Layer
    {
    public:
        virtual ~Layer() = default;

        virtual bool OnAttach(Application& application);
        virtual void OnDetach();
        virtual void OnFixedUpdate(const TimeStep& timeStep);
        virtual void OnUpdate(const TimeStep& timeStep);
        virtual void OnRender(Renderer& renderer);
        virtual void OnGui();
        virtual void OnResize(unsigned int width, unsigned int height);
    };

    inline bool Layer::OnAttach(Application& application)
    {
        (void)application;
        return true;
    }

    inline void Layer::OnDetach()
    {
    }

    inline void Layer::OnFixedUpdate(const TimeStep& timeStep)
    {
        (void)timeStep;
    }

    inline void Layer::OnUpdate(const TimeStep& timeStep)
    {
        (void)timeStep;
    }

    inline void Layer::OnRender(Renderer& renderer)
    {
        (void)renderer;
    }

    inline void Layer::OnGui()
    {
    }

    inline void Layer::OnResize(unsigned int width, unsigned int height)
    {
        (void)width;
        (void)height;
    }
}
