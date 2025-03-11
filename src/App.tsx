import {
  HashRouter as Router,
  Route,
  Routes,
  useLocation,
  useNavigate,
} from "react-router-dom";
import { motion, AnimatePresence } from "framer-motion";
import { Facebook, Instagram, Youtube, Apple as WhatsApp } from "lucide-react";
import { Link } from "react-router-dom";
import { useEffect } from "react";

import Courses from "./pages/Courses";
import AcercaDe from "./pages/Acercade";
import MainPage from "./pages/main_page";

function AnimatedRoutes() {
  const location = useLocation();
  const navigate = useNavigate();

  // useEffect(() => {
  //   if (location.state?.reload) {
  //     window.location.href = location.pathname;
  //   }
  // }, [location]);

  return (
    <AnimatePresence
      onExitComplete={() => {
        navigate(location.pathname, { state: { reload: true } });
      }}
    >
      <Routes location={location} key={location.pathname}>
        <Route
          path="/cursos"
          element={
            <motion.div
              initial={{ opacity: 0, scale: 0.9 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.9 }}
              transition={{ duration: 0.5 }}
              className="page-transition"
            >
              <Courses />
            </motion.div>
          }
        />
        <Route
          path="/acerca-de"
          element={
            <motion.div
              initial={{ opacity: 0, scale: 0.9 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.9 }}
              transition={{ duration: 0.5 }}
              className="page-transition"
            >
              <AcercaDe />
            </motion.div>
          }
        />
        <Route
          path="/"
          element={
            <motion.div
              initial={{ opacity: 0, scale: 0.9 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.9 }}
              transition={{ duration: 0.5 }}
              className="page-transition"
            >
              <MainPage />
            </motion.div>
          }
        />
      </Routes>
    </AnimatePresence>
  );
}

function App() {
  return (
    <Router>
      <div className="min-h-screen bg-black text-white">
        {/* Background Overlay */}
        <div
          className="fixed inset-0 w-full h-full z-0"
          style={{
            backgroundImage:
              "url('https://images.unsplash.com/photo-1634261324971-1b0ddbad73de?w=500&auto=format&fit=crop&q=60&ixlib=rb-4.0.3&ixid=M3wxMjA3fDB8MHxzZWFyY2h8MTc0fHx0YXJvdHxlbnwwfHwwfHx8MA%3D%3D')",
            backgroundSize: "cover",
            backgroundPosition: "center",
            backgroundRepeat: "no-repeat",
            opacity: "0.15",
          }}
        />

        {/* Navbar */}
        <nav className="fixed top-0 w-full bg-black/95 backdrop-blur-sm border-b border-white/10 z-50">
          <div className="container mx-auto px-4 py-4">
            <div className="flex items-center justify-between">
              <Link to="/" className="text-xl font-bold tracking-[0.2em]">
                <motion.div
                  initial={{ scale: 1 }}
                  whileHover={{ scale: 1.1 }}
                  whileTap={{ scale: 0.9 }}
                  transition={{ duration: 0.2 }}
                >
                  VIBRACIÓN Y BIENESTAr
                </motion.div>
              </Link>

              <div className="hidden md:flex items-center space-x-8">
                <Link
                  to="/acerca-de"
                  className="hover:text-gray-300 transition-all duration-300"
                >
                  Acerca de
                </Link>
                <Link
                  to="/cursos"
                  className="hover:text-gray-300 transition-all duration-300"
                >
                  Cursos
                </Link>
                <a
                  href="#"
                  className="hover:text-gray-300 transition-all duration-300"
                >
                  Blog
                </a>
                <a
                  href="#"
                  className="hover:text-gray-300 transition-all duration-300"
                >
                  Contacto
                </a>
                <a
                  href="#"
                  className="hover:text-gray-300 transition-all duration-300"
                >
                  Tienda
                </a>
              </div>

              <div className="flex items-center space-x-4">
                <button className="px-4 py-2 border border-white/80 rounded-md hover:bg-white hover:text-black transition-all duration-300">
                  Iniciar sesión
                </button>
                <div className="flex items-center space-x-3">
                  <a
                    href="https://instagram.com"
                    target="_blank"
                    rel="noopener noreferrer"
                    className="hover:text-gray-300 transition-all duration-300"
                  >
                    <Instagram size={20} />
                  </a>
                  <a
                    href="https://youtube.com"
                    target="_blank"
                    rel="noopener noreferrer"
                    className="hover:text-gray-300 transition-all duration-300"
                  >
                    <Youtube size={20} />
                  </a>
                  <a
                    href="https://facebook.com"
                    target="_blank"
                    rel="noopener noreferrer"
                    className="hover:text-gray-300 transition-all duration-300"
                  >
                    <Facebook size={20} />
                  </a>
                  <a
                    href={`https://wa.me/${
                      import.meta.env.REACT_APP_WHATSAPP_NUMBER
                    }`}
                    target="_blank"
                    rel="noopener noreferrer"
                    className="hover:text-gray-300 transition-all duration-300"
                  >
                    <WhatsApp size={20} />
                  </a>
                </div>
              </div>
            </div>
          </div>
        </nav>

        {/* Routes with animations */}
        <AnimatedRoutes />

        {/* Warning Message */}
        <div className="container mx-auto px-4 py-16 text-center">
          <p className="text-lg font-bold max-w-3xl mx-auto text-gray-400">
            Denuncia si te ofrecen cursos desde otro número suplantando nuestra
            identidad. Nuestro único número de WhatsApp es{" "}
            <span className="text-white">6624653737</span>.
          </p>
        </div>
      </div>
    </Router>
  );
}

export default App;
