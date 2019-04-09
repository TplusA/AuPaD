/*
 * Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of AuPaD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef TADDYBUS_HH
#define TADDYBUS_HH

#include "messages.h"

#include <string>
#include <unordered_map>
#include <list>
#include <functional>

#include <gio/gio.h>

/*!
 * T+A D-Bus (wrapping GDBus into a sensible C++ interface).
 */
namespace TDBus
{

bool log_dbus_error(GError **error, const char *what);

/*!
 * Base class for server-side D-Bus interfaces implementations.
 */
class IfaceBase
{
  protected:
    std::unordered_map<std::string, void *> user_data_pointers_;
    const std::string object_path_;

    explicit IfaceBase(std::string &&object_path):
        object_path_(std::move(object_path))
    {}

  public:
    IfaceBase(const IfaceBase &) = delete;
    IfaceBase(IfaceBase &&) = default;
    IfaceBase &operator=(const IfaceBase &) = delete;
    IfaceBase &operator=(IfaceBase &&) = default;

    virtual ~IfaceBase() = default;

    template <typename T>
    T *data(GDBusMethodInvocation *invocation) const
    {
        return static_cast<T *>(get_data_pointer(invocation));
    }

    /*!
     * Export an object implementing this interface on the given connection.
     */
    bool export_interface(GDBusConnection *connection) const
    {
        GError *error = nullptr;
        if(g_dbus_interface_skeleton_export(get_interface_skeleton(),
                                            connection, object_path_.c_str(),
                                            &error))
            return true;

        /* TODO: handle error */
        return false;
    }

    const std::string &get_object_path() const { return object_path_; }

    static void method_fail(GDBusMethodInvocation *invocation,
                            const char *message, ...);

  protected:
    virtual GDBusInterfaceSkeleton *get_interface_skeleton() const = 0;

  private:
    void *get_data_pointer(GDBusMethodInvocation *invocation) const
    {
        const auto it(user_data_pointers_.find(
                        g_dbus_method_invocation_get_method_name(invocation)));
        return it == user_data_pointers_.end() ? nullptr : it->second;
    }
};

/*!
 * Traits class for server-side D-Bus interface implementation.
 *
 * For use with #TDBus::Iface.
 */
template <typename T> struct IfaceTraits;

/*!
 * Traits class for server-side D-Bus method implementation.
 *
 * For use with #TDBus::Iface.
 */
template <typename T> struct MethodHandlerTraits;

/*!
 * Traits class for client-side D-Bus signal implementation.
 *
 * For use with #TDBus::Iface.
 */
template <typename T> struct SignalHandlerTraits;

/*!
 * Template for server-side D-Bus interfaces implementations.
 *
 * \tparam T
 *     A type "derived" from \c GDBusInterfaceSkeleton.
 */
template <typename T, typename Traits = IfaceTraits<T>>
class Iface: public IfaceBase
{
  private:
    T *iface_;

  public:
    explicit Iface(std::string &&object_path):
        IfaceBase(std::move(object_path)),
        iface_(Traits::skeleton_new())
    {}

    ~Iface()
    {
        if(iface_ == nullptr)
            return;

        g_object_unref(iface_);
        iface_ = nullptr;
    }

    /*!
     * Connect D-Bus method invocations to its handler.
     *
     * Calling this function means that the application is willing to handle
     * D-Bus calls of the method associated with \p MHTraits. The caller must
     * therefore provide an implementation of the handler declared in
     * \p MHTraits.
     *
     * Note that the handler is shared among \e all connections and \e all
     * objects implementing the D-Bus interface.
     *
     * \tparam Tag
     *     A type used to identify a D-Bus method, usually a \c struct name
     *     with no definition (incomplete type).
     *
     * \tparam MHTraits
     *     Handler traits declaring the D-Bus method handler.
     *
     * \param user_data
     *     Optional pointer for use inside the handler. Use
     *     #TDBus::IfaceBase::data() to get the pointer.
     */
    template <typename Tag, typename MHTraits = MethodHandlerTraits<Tag>>
    void connect_method_handler(void *user_data = nullptr)
    {
        static_assert(std::is_same<typename MHTraits::IfaceType, T>::value,
                      "Handler is not meant to be used with this interface");
        user_data_pointers_[MHTraits::dbus_method_name()] = user_data;
        g_signal_connect(iface_, MHTraits::glib_signal_name(),
                         G_CALLBACK(MHTraits::handler), this);
    }

    /*!
     * Complete handling a D-Bus method call.
     *
     * This function is called from D-Bus method handlers, either directly
     * inside the handler or deferred in different context.
     *
     * \tparam Tag
     *     A type used to identify a D-Bus method, usually a \c struct name
     *     with no definition (incomplete type).
     *
     * \tparam MHTraits
     *     Callers shall pass \c ThisMethod, unqualified so if used directly in
     *     D-Bus handlers, or qualifying it with the correct traits class in
     *     other contexts. In case of a call directly from the D-Bus handler,
     *     the traits class will be in scope and the correct type named
     *     \c ThisMethod is going to be picked up from there.
     *
     * \tparam Args
     *     Any arguments the completion function requires.
     *
     * \param invocation
     *     The D-Bus method invocation as passed in from GLib.
     *
     *  \param args
     *     Forwarded to completion function.
     */
    template <typename Tag, typename MHTraits = MethodHandlerTraits<Tag>,
              typename... Args>
    void method_done(GDBusMethodInvocation *invocation, Args &&... args)
    {
        static_assert(std::is_same<typename MHTraits::IfaceType, T>::value,
                      "Handler is not meant to be used with this interface");
        MHTraits::complete(iface_, invocation, std::forward<Args>(args)...);
    }

    void sanitize(const T *object, GDBusMethodInvocation *invocation)
    {
        log_assert(object == iface_);
        log_assert(g_dbus_method_invocation_get_object_path(invocation) == object_path_);
    }

  protected:
    GDBusInterfaceSkeleton *get_interface_skeleton() const final override
    {
        return G_DBUS_INTERFACE_SKELETON(iface_);
    }
};

/*!
 * Base class for client-side proxy objects.
 */
class ProxyBase
{
  public:
    using ProxyNewFunction =
        void (*)(GDBusConnection *, GDBusProxyFlags, const char *, const char *,
                 GCancellable *, GAsyncReadyCallback, gpointer);

    template <typename T>
    using ProxyNewFinishFunction = T *(*)(GAsyncResult *res, GError **error);

  protected:
    const std::string name_;
    const std::string object_path_;
    bool is_busy_ = false;

    std::unordered_map<std::string, void *> user_data_pointers_;

    explicit ProxyBase(std::string &&name, std::string &&object_path):
        name_(std::move(name)),
        object_path_(std::move(object_path)),
        is_busy_(false)
    {}

  public:
    ProxyBase(const ProxyBase &) = delete;
    ProxyBase(ProxyBase &&) = default;
    ProxyBase &operator=(const ProxyBase &) = delete;
    ProxyBase &operator=(ProxyBase &&) = default;
    virtual ~ProxyBase() = default;
};

template <typename T> struct ProxyTraits;

/*!
 * Client-side proxy for remote object.
 *
 * \tparam T
 *     A type "derived" from \c GDBusProxy.
 */
template <typename T, typename Traits = ProxyTraits<T>>
class Proxy: public ProxyBase
{
  private:
    std::function<void(Proxy &proxy, bool)> notify_;
    T *proxy_;

  public:
    explicit Proxy(std::string &&name, std::string &&object_path):
        ProxyBase(std::move(name), std::move(object_path)),
        proxy_(nullptr)
    {}

    void connect_proxy(GDBusConnection *connection,
                       std::function<void(Proxy &proxy, bool)> &&notify = nullptr)
    {
        if(is_busy_)
        {
            BUG("Cannot create proxy for D-Bus object %s at %s while busy",
                object_path_.c_str(), name_.c_str());
            return;
        }

        if(proxy_ != nullptr)
            return;

        notify_ = std::move(notify);
        is_busy_ = true;

        (*Traits::proxy_new_fn())(connection, G_DBUS_PROXY_FLAGS_NONE,
                                  name_.c_str(), object_path_.c_str(),
                                  nullptr, connect_done, this);
    }

    /*!
     * Connect D-Bus signal reception to its handler.
     *
     * Like #TDBus::Iface::connect_method_handler(), but for D-Bus signals and
     * using #TDBus::SignalHandlerTraits.
     */
    template <typename Tag, typename SHTraits = SignalHandlerTraits<Tag>>
    void connect_signal_handler(void *user_data = nullptr)
    {
        static_assert(std::is_same<typename SHTraits::IfaceType, T>::value,
                      "Handler is not meant to be used with this interface");
        user_data_pointers_[SHTraits::dbus_signal_name()] = user_data;
        g_signal_connect(proxy_, SHTraits::glib_signal_name(),
                         G_CALLBACK(SHTraits::handler), this);
    }

  private:
    static void connect_done(GObject *source_object, GAsyncResult *res,
                             gpointer user_data)
    {
        GError *error = nullptr;
        auto proxy = (*Traits::proxy_new_finish_fn())(res, &error);
        const bool result = log_dbus_error(&error, "Create D-Bus proxy");
        static_cast<Proxy *>(user_data)->ready(proxy, result);
    }

    void ready(T *proxy, bool result)
    {
        proxy_ = proxy;
        is_busy_ = false;

        if(notify_ == nullptr)
            return;

        const auto notify(std::move(notify_));
        notify(*this, result);
    }
};

/*!
 * Observe presence of a specific name on a D-Bus connection.
 *
 * While it is perfectly possible to use watchers directly in client code,
 * using #TDBus::Bus::add_watcher() is simpler and often sufficient.
 */
class PeerWatcher
{
  private:
    const std::string name_;
    const std::function<void(GDBusConnection *, const char *)> name_appeared_;
    const std::function<void(GDBusConnection *, const char *)> name_vanished_;

    guint watcher_id_;

  public:
    PeerWatcher(const PeerWatcher &) = delete;
    PeerWatcher(PeerWatcher &&) = default;
    PeerWatcher &operator=(const PeerWatcher &) = delete;
    PeerWatcher &operator=(PeerWatcher &&) = default;

    explicit PeerWatcher(const char *name,
                         std::function<void(GDBusConnection *, const char *)> &&name_appeared,
                         std::function<void(GDBusConnection *, const char *)> &&name_vanished):
        name_(name),
        name_appeared_(std::move(name_appeared)),
        name_vanished_(std::move(name_vanished)),
        watcher_id_(0)
    {}

    ~PeerWatcher() { stop(); }

    void start(GDBusConnection *connection);
    void stop();

  private:
    static void appeared(GDBusConnection *connection,
                         const gchar *name, const gchar *name_owner,
                         gpointer user_data);
    static void vanished(GDBusConnection *connection,
                         const gchar *name, gpointer user_data);
};

/*!
 * D-Bus connection.
 */
class Bus
{
  public:
    enum class Type
    {
        SESSION,
        SYSTEM,
    };

  private:
    const std::string object_name_;
    const Type bus_type_;

    std::function<void(GDBusConnection *)> on_connect_;
    std::function<void(GDBusConnection *)> on_name_acquired_;
    std::function<void(GDBusConnection *)> on_name_lost_;

    guint owner_id_;
    std::list<PeerWatcher> watchers_;
    std::list<const IfaceBase *> interfaces_;

  public:
    Bus(const Bus &) = delete;
    Bus(Bus &&) = default;
    Bus &operator=(const Bus &) = delete;
    Bus &operator=(Bus &&) = default;

    explicit Bus(const char *object_name, Type t);
    ~Bus();

    void add_watcher(const char *name,
                     std::function<void(GDBusConnection *, const char *)> &&appeared,
                     std::function<void(GDBusConnection *, const char *)> &&vanished)
    {
        watchers_.emplace_back(name, std::move(appeared), std::move(vanished));
    }

    void add_auto_exported_interface(const IfaceBase &iface)
    {
        interfaces_.push_back(&iface);
    }

    bool connect(std::function<void(GDBusConnection *)> &&on_connect,
                 std::function<void(GDBusConnection *)> &&on_name_acquired,
                 std::function<void(GDBusConnection *)> &&on_name_lost);

  private:
    static void bus_acquired(GDBusConnection *connection,
                             const gchar *name, gpointer user_data);
    static void name_acquired(GDBusConnection *connection,
                              const gchar *name, gpointer user_data);
    static void name_lost(GDBusConnection *connection,
                          const gchar *name, gpointer user_data);
};

}

#endif /* !TADDYBUS_HH */
