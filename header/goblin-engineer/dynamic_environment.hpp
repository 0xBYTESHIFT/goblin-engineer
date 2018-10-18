#pragma once

#include <actor-zeta/actor/actor_address.hpp>

#include <goblin-engineer/forward.hpp>
#include <goblin-engineer/configuration.hpp>
#include <goblin-engineer/context.hpp>

namespace goblin_engineer {

    class dynamic_environment final :
            public context_t,
            public abstract_environment {
    public:

        dynamic_environment(configuration&&);

        ~dynamic_environment();

        template <typename SERVICE,typename ...Args>
        auto add_service(Args &&...args) -> service& {
            return add_service(new SERVICE(static_cast<context_t*>(this),std::forward<Args>(args)...));
        }

        template<typename DataProvider,typename ...Args>
        auto add_data_provider(actor_zeta::actor::actor_address address,Args &&...args) -> data_provider& {
            return add_data_provider(new DataProvider(static_cast<context_t*>(this),std::move(address),std::forward<Args>(args)...));
        }

        auto add_plugin(abstract_plugin *) -> void;

        void initialize();

        void startup();

        void shutdown();

    private:

        auto env() -> actor_zeta::environment::abstract_environment *;

        auto add_service(abstract_service*) ->  service &;

        auto add_data_provider(data_provider*)-> data_provider&;

        auto  config()  const -> dynamic_config& override;

        int start() override ;

        auto manager_execution_device() -> actor_zeta::executor::abstract_coordinator & override ;

        auto manager_group() -> actor_zeta::environment::cooperation & override ;

        auto context() -> context_t *;

        boost::asio::io_service& main_loop() const override;

        boost::thread_group& background() const override;

        struct impl;

        std::unique_ptr<impl> pimpl;
    };

    inline auto add_plugin(dynamic_environment &app, abstract_plugin* plugin ) -> void {
        app.add_plugin(plugin);
    }

}

