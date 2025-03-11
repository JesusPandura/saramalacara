import React from "react";
import Carousel from "../components/Carousel";

const tarotImages = [
  "/images/curso tarot_revolucion.png",
  "/images/magia con velas.png",
  "/images/seccion.png",
  "/images/oferta1.png",
  "/images/oferta2.png",
];

const MainPage = () => {
  return (
    <div className="min-h-screen text-white">
      {/* Hero Section */}
      <div className="pt-32 pb-16 px-4">
        <div className="container mx-auto text-center relative">
          {/* Mystical Sun and Moon */}
          <div className=" absolute inset-0 flex items-center justify-center pointer-events-none">
            <div className="w-[1200px] h-[1200px]">
              {/* Sun */}
              <div className="absolute inset-0 animate-[spin_60s_linear_infinite]">
                <div
                  className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[1200px] h-[1200px] 
                    border border-white/10 rounded-full"
                >
                  {/* Sun Rays */}
                  {[...Array(12)].map((_, i) => (
                    <div
                      key={i}
                      className="absolute top-1/2 left-1/2 w-1 h-[1180px] origin-center"
                      style={{
                        transform: `translate(-50%, -50%) rotate(${i * 30}deg)`,
                      }}
                    >
                      <div className="w-1 h-16 bg-gradient-to-b from-white/20 to-transparent absolute top-0" />
                      <div className="w-1 h-16 bg-gradient-to-t from-white/20 to-transparent absolute bottom-0" />
                    </div>
                  ))}
                </div>
              </div>

              {/* Moon */}
              <div className="absolute inset-0 animate-[spin_40s_linear_infinite_reverse]">
                <div
                  className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[900px] h-[900px] 
                    border border-white/20 rounded-full overflow-hidden"
                >
                  {/* Moon Craters */}
                  <div className="absolute top-[20%] left-[30%] w-20 h-20 rounded-full border border-white/20" />
                  <div className="absolute top-[40%] left-[60%] w-28 h-28 rounded-full border border-white/20" />
                  <div className="absolute top-[60%] left-[20%] w-16 h-16 rounded-full border border-white/20" />
                </div>
              </div>
            </div>
          </div>

          <h1 className="text-4xl md:text-6xl font-bold leading-tight mb-8 relative tracking-wider">
            APRENDE TAROT
            <br />
            DE MANERA CONSCIENTE,
            <br />
            <span className="text-gray-400">ÉTICA Y SEGURA.</span>
          </h1>
          <button
            className="px-8 py-4 bg-white text-black rounded-full text-lg font-bold 
              hover:bg-gray-200 transition-all duration-300 transform hover:scale-105 relative"
          >
            INSCRÍBETE A NUESTROS CURSOS
          </button>
        </div>
      </div>

      {/* Carousel Section */}
      <div className="container mt-10 mx-auto px-4 py-16">
        <Carousel images={tarotImages} height="600px" width="1000px" />
      </div>

      {/* Features Grid */}
      <div className="container mx-auto px-4 py-16">
        <div className="grid md:grid-cols-3 gap-8">
          <div className="p-8 border border-white/10 rounded-lg backdrop-blur-sm bg-black/50 hover:border-white/30 transition-all duration-500 group">
            <h3 className="text-xl font-bold mb-4 tracking-wide group-hover:translate-x-2 transition-transform duration-300">
              Arcanos Mayores
            </h3>
            <p className="text-gray-400 group-hover:text-gray-300 transition-colors duration-300">
              Descubre los secretos de las 22 cartas fundamentales del Tarot y
              su profundo significado espiritual.
            </p>
          </div>
          <div className="p-8 border border-white/10 rounded-lg backdrop-blur-sm bg-black/50 hover:border-white/30 transition-all duration-500 group">
            <h3 className="text-xl font-bold mb-4 tracking-wide group-hover:translate-x-2 transition-transform duration-300">
              Lecturas Intuitivas
            </h3>
            <p className="text-gray-400 group-hover:text-gray-300 transition-colors duration-300">
              Desarrolla tu intuición y aprende a interpretar las cartas más
              allá de sus significados tradicionales.
            </p>
          </div>
          <div className="p-8 border border-white/10 rounded-lg backdrop-blur-sm bg-black/50 hover:border-white/30 transition-all duration-500 group">
            <h3 className="text-xl font-bold mb-4 tracking-wide group-hover:translate-x-2 transition-transform duration-300">
              Rituales y Energía
            </h3>
            <p className="text-gray-400 group-hover:text-gray-300 transition-colors duration-300">
              Aprende a conectar con la energía de las cartas y crear rituales
              significativos en tus lecturas.
            </p>
          </div>
        </div>
      </div>
    </div>
  );
};

export default MainPage;
