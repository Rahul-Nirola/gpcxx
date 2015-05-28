/*
 * koza.cpp
 * Date: 2015-05-28
 * Author: Karsten Ahnert (karsten.ahnert@gmx.de)
 * Copyright: Karsten Ahnert
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or
 * copy at http://www.boost.org/LICENSE_1_0.txt)
 */



#include <gpcxx/tree.hpp>
#include <gpcxx/generate.hpp>
#include <gpcxx/operator.hpp>
#include <gpcxx/eval.hpp>
#include <gpcxx/evolve.hpp>
#include <gpcxx/io.hpp>
#include <gpcxx/stat.hpp>
#include <gpcxx/app.hpp>


#include <iostream>
#include <random>
#include <vector>
#include <functional>

template< typename Rng , typename F >
gpcxx::regression_training_data< double , 1 > generate_test_data( Rng& rng , size_t  num_of_points , F f )
{
    gpcxx::regression_training_data< double , 1 > c;
    std::uniform_real_distribution< double > dist( -1.0 , 1.0 );
    for( size_t i=0 ; i<20 ; ++i )
    {
        double x = dist( rng );
        c.y.push_back( f( x ) );
        c.x[0].push_back( x );
    }
    return c;
}



int main( int argc , char** argv )
{
    using rng_type = std::mt19937;
    rng_type rng;

//     auto c = generate_test_data( rng , 20 , []( double x )
//             { return  x * x * x * x + x * x * x + x * x + x; } );

    auto c = generate_test_data( rng , 20 , []( double x )
            { return  x * x * x * x + x * x * x + x * x + x; } );

    
    // define_tree_types
    using context_type = gpcxx::regression_context< double , 1 >;
    using node_type = gpcxx::intrusive_named_func_node< double , const context_type > ;
    using tree_type = gpcxx::intrusive_tree< node_type >;
    
    
    // define node types
    auto terminal_gen = gpcxx::make_uniform_symbol( std::vector< node_type >{
        node_type { gpcxx::array_terminal< 0 >{}                                     ,      "x" }
    } );
    auto unary_gen = gpcxx::make_uniform_symbol( std::vector< node_type > {
        node_type { gpcxx::sin_func {}                                               ,      "sin" } ,
        node_type { gpcxx::cos_func {}                                               ,      "cos" } ,
        node_type { gpcxx::exp_func {}                                               ,      "exp" } ,
        node_type { gpcxx::log_func {}                                               ,      "log" }
    } );
    auto binary_gen = gpcxx::make_uniform_symbol( std::vector< node_type > {
        node_type { gpcxx::plus_func {}                                              ,      "+" } ,
        node_type { gpcxx::minus_func {}                                             ,      "-" } ,
        node_type { gpcxx::multiplies_func {}                                        ,      "*" } ,
        node_type { gpcxx::divides_func {}                                           ,      "/" }
    } );


    auto node_generator = gpcxx::node_generator< node_type , rng_type , 3 > {
        { 1.0 , 0 , terminal_gen } ,
        { 1.0 , 1 , unary_gen } ,
        { 1.0 , 2 , binary_gen } };
        
    // define_gp_parameters
    size_t population_size = 512;
    size_t generation_size = 50;
    size_t number_elite = 1;
    double mutation_rate = 0.2;
    double crossover_rate = 0.6;
    double reproduction_rate = 0.3;
    size_t min_tree_height = 4 , max_tree_height = 12;
    size_t tournament_size = 15;
        
    // define_population_and_fitness
    using population_type = std::vector< tree_type >;
    using fitness_type = std::vector< double >;
    
    fitness_type fitness( population_size , 0.0 );
    population_type population( population_size );

    // define_evolution
    using evolver_type = gpcxx::static_pipeline< population_type , fitness_type , rng_type >;
    evolver_type evolver( number_elite , mutation_rate , crossover_rate , reproduction_rate , rng );


    // define_evaluator
    using evaluator = struct {
        using context_type = gpcxx::regression_context< double , 1 >;
        using value_type = double;
        value_type operator()( tree_type const& t , context_type const& c ) const {
            return t.root()->eval( c );
        } };


    // define_genetic_operators
    auto tree_generator = gpcxx::make_ramp( rng , node_generator , min_tree_height , max_tree_height , 0.5 );
    auto fitness_f = gpcxx::make_regression_fitness( evaluator {} );
    evolver.mutation_function() = gpcxx::make_mutation(
        gpcxx::make_point_mutation( rng , tree_generator , max_tree_height , 20 ) ,
        gpcxx::make_tournament_selector( rng , tournament_size ) );
    evolver.crossover_function() = gpcxx::make_crossover( 
        gpcxx::make_one_point_crossover_strategy( rng , 10 ) ,
        gpcxx::make_tournament_selector( rng , tournament_size ) );
    evolver.reproduction_function() = gpcxx::make_reproduce( gpcxx::make_tournament_selector( rng , tournament_size ) );

    // init_population
    for( size_t i=0 ; i<population.size() ; ++i )
    {
        tree_generator( population[i] );
        fitness[i] = fitness_f( population[i] , c );
    }

    std::cout << "Best individuals" << std::endl << gpcxx::best_individuals( population , fitness ) << std::endl;
    std::cout << "Statistics : " << gpcxx::calc_population_statistics( population ) << std::endl;
    std::cout << std::endl << std::endl;

    // main_loop
    for( size_t i=0 ; i<generation_size ; ++i )
    {
        evolver.next_generation( population , fitness );
        for( size_t i=0 ; i<population.size() ; ++i )
            fitness[i] = fitness_f( population[i] , c );
        
        std::cout << "Iteration " << i << std::endl;
        std::cout << "Best individuals" << std::endl << gpcxx::best_individuals( population , fitness , 1 ) << std::endl;
        std::cout << "Statistics : " << gpcxx::calc_population_statistics( population ) << std::endl << std::endl;
    }

    return 0;
}